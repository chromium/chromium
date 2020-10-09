// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/devtools_listener.h"

#include <stddef.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "url/url_util.h"

namespace file_manager {

namespace {

base::span<const uint8_t> StringToSpan(const std::string& str) {
  return base::as_bytes(base::make_span(str));
}

std::string EncodedURL(const std::string& url) {
  url::RawCanonOutputT<char> canonical_url;
  url::EncodeURIComponent(url.c_str(), url.size(), &canonical_url);
  return std::string(canonical_url.data(), canonical_url.length());
}

}  // namespace

DevToolsListener::DevToolsListener(content::DevToolsAgentHost* host,
                                   uint32_t uuid)
    : uuid_(base::StringPrintf("%u", uuid)) {
  CHECK(!host->IsAttached());
  host->AttachClient(this);
  Start(host);
}

DevToolsListener::~DevToolsListener() = default;

void DevToolsListener::Navigated(content::DevToolsAgentHost* host) {
  CHECK(host->IsAttached() && attached_);
  navigated_ = StartJSCoverage(host);
}

bool DevToolsListener::HasCoverage(content::DevToolsAgentHost* host) {
  return attached_ && navigated_;
}

void DevToolsListener::GetCoverage(content::DevToolsAgentHost* host,
                                   const base::FilePath& store,
                                   const std::string& test) {
  if (HasCoverage(host))
    StopAndStoreJSCoverage(host, store, test);
  navigated_ = false;
}

void DevToolsListener::Detach(content::DevToolsAgentHost* host) {
  if (attached_)
    host->DetachClient(this);
  navigated_ = false;
  attached_ = false;
}

std::string DevToolsListener::HostString(content::DevToolsAgentHost* host,
                                         const std::string& prefix = "") {
  std::string result = base::StrCat(
      {prefix, " ", host->GetType(), " title: ", host->GetTitle()});
  std::string description = host->GetDescription();
  if (!description.empty())
    base::StrAppend(&result, {" description: ", description});
  std::string url = host->GetURL().spec();
  if (!url.empty())
    base::StrAppend(&result, {" URL: ", url});
  return result;
}

void DevToolsListener::Start(content::DevToolsAgentHost* host) {
  std::string enable_runtime = "{\"id\":10,\"method\":\"Runtime.enable\"}";
  host->DispatchProtocolMessage(this, StringToSpan(enable_runtime));

  std::string enable_page = "{\"id\":11,\"method\":\"Page.enable\"}";
  host->DispatchProtocolMessage(this, StringToSpan(enable_page));
}

bool DevToolsListener::StartJSCoverage(content::DevToolsAgentHost* host) {
  std::string enable_profiler = "{\"id\":20,\"method\":\"Profiler.enable\"}";
  host->DispatchProtocolMessage(this, StringToSpan(enable_profiler));

  std::string start_precise_coverage =
      "{\"id\":21,\"method\":\"Profiler.startPreciseCoverage\",\"params\":{"
      "\"callCount\":true,\"detailed\":true}}";
  host->DispatchProtocolMessage(this, StringToSpan(start_precise_coverage));

  std::string enable_debugger = "{\"id\":22,\"method\":\"Debugger.enable\"}";
  host->DispatchProtocolMessage(this, StringToSpan(enable_debugger));

  std::string skip_pauses =
      "{\"id\":23,\"method\":\"Debugger.setSkipAllPauses\""
      ",\"params\":{\"skip\":true}}";
  host->DispatchProtocolMessage(this, StringToSpan(skip_pauses));

  return true;
}

void DevToolsListener::StopAndStoreJSCoverage(content::DevToolsAgentHost* host,
                                              const base::FilePath& store,
                                              const std::string& test) {
  std::string precise_coverage =
      "{\"id\":40,\"method\":\"Profiler.takePreciseCoverage\"}";
  host->DispatchProtocolMessage(this, StringToSpan(precise_coverage));
  AwaitMessageResponse(40);

  script_coverage_.reset(value_.release());
  StoreScripts(host, store);

  std::string debugger = "{\"id\":41,\"method\":\"Debugger.disable\"}";
  host->DispatchProtocolMessage(this, StringToSpan(debugger));

  std::string profiler = "{\"id\":42,\"method\":\"Profiler.disable\"}";
  host->DispatchProtocolMessage(this, StringToSpan(profiler));

  base::DictionaryValue* result = nullptr;
  CHECK(script_coverage_->GetDictionary("result", &result));

  base::ListValue* coverage_entries = nullptr;
  CHECK(result->GetList("result", &coverage_entries));

  auto entries = std::make_unique<base::ListValue>();
  for (size_t i = 0; i != coverage_entries->GetSize(); ++i) {
    base::DictionaryValue* entry = nullptr;
    CHECK(coverage_entries->GetDictionary(i, &entry));

    std::string script_id;
    CHECK(entry->GetString("scriptId", &script_id));
    const auto it = script_id_map_.find(script_id);
    if (it == script_id_map_.end())
      continue;

    CHECK(entry->SetString("hash", it->second));
    entries->Append(entry->CreateDeepCopy());
  }

  const std::string url = host->GetURL().spec();
  CHECK(result->SetString("encodedHostURL", EncodedURL(url)));
  CHECK(result->SetString("hostTitle", host->GetTitle()));
  CHECK(result->SetString("hostType", host->GetType()));
  CHECK(result->SetString("hostTest", test));
  CHECK(result->SetString("hostURL", url));

  const std::string md5 = base::MD5String(HostString(host, test));
  std::string coverage = base::StrCat({test, ".", md5, uuid_, ".js.json"});
  base::FilePath path = store.AppendASCII("tests").Append(coverage);

  CHECK(result->SetList("result", std::move(entries)));
  CHECK(base::JSONWriter::Write(*result, &coverage));
  base::WriteFile(path, coverage.data(), coverage.size());

  script_coverage_.reset();
  script_hash_map_.clear();
  script_id_map_.clear();
  script_.clear();

  AwaitMessageResponse(42);
  value_.reset();
}

void DevToolsListener::StoreScripts(content::DevToolsAgentHost* host,
                                    const base::FilePath& store) {
  for (size_t i = 0; i < script_.size(); ++i, value_.reset()) {
    std::string id;
    CHECK(script_[i]->GetString("params.scriptId", &id));
    CHECK(!id.empty());

    std::string url;
    if (!script_[i]->GetString("params.url", &url))
      script_[i]->GetString("params.sourceURL", &url);
    if (url.empty())
      continue;

    std::string script_source = base::StringPrintf(
        "{\"id\":50,\"method\":\"Debugger.getScriptSource\""
        ",\"params\":{\"scriptId\":\"%s\"}}",
        id.c_str());
    host->DispatchProtocolMessage(this, StringToSpan(script_source));
    AwaitMessageResponse(50);

    base::DictionaryValue* result = nullptr;
    CHECK(value_->GetDictionary("result", &result));
    std::string text;
    result->GetString("scriptSource", &text);
    if (text.empty())
      continue;

    std::string hash;
    CHECK(script_[i]->GetString("params.hash", &hash));
    if (script_id_map_.find(id) != script_id_map_.end())
      LOG(FATAL) << "Duplicate script by id " << url;
    script_id_map_[id] = hash;
    CHECK(!hash.empty());
    if (script_hash_map_.find(hash) != script_hash_map_.end())
      continue;
    script_hash_map_[hash] = id;

    base::DictionaryValue* script = nullptr;
    CHECK(script_[i]->GetDictionary("params", &script));
    CHECK(script->SetString("encodedURL", EncodedURL(url)));
    CHECK(script->SetString("hash", hash));
    CHECK(script->SetString("text", text));
    CHECK(script->SetString("url", url));

    base::FilePath path = store.AppendASCII(hash.append(".js.json"));
    CHECK(base::JSONWriter::Write(*script, &text));
    if (!base::PathExists(path))  // Deduplication
      base::WriteFile(path, text.data(), text.size());
  }
}

void DevToolsListener::AwaitMessageResponse(int id) {
  value_.reset();
  value_id_ = id;

  base::RunLoop run_loop;
  value_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void DevToolsListener::DispatchProtocolMessage(
    content::DevToolsAgentHost* host,
    base::span<const uint8_t> span_message) {
  if (!navigated_)
    return;

  std::string message(reinterpret_cast<const char*>(span_message.data()),
                      span_message.size());

  std::unique_ptr<base::DictionaryValue> response =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(message));
  CHECK(response);

  std::string* method = response->FindStringPath("method");
  if (method) {
    if (*method == "Debugger.scriptParsed")
      script_.push_back(std::move(response));
    else if (*method == "Runtime.executionContextsCreated")
      script_.clear();
    return;
  }

  base::Optional<int> id = response->FindIntPath("id");
  if (id.has_value() && id.value() == value_id_) {
    value_.reset(response.release());
    CHECK(value_closure_);
    std::move(value_closure_).Run();
  }
}

bool DevToolsListener::MayAttachToURL(const GURL& url, bool is_webui) {
  return true;
}

void DevToolsListener::AgentHostClosed(content::DevToolsAgentHost* host) {
  CHECK(!value_closure_);
  navigated_ = false;
  attached_ = false;
}

}  // namespace file_manager

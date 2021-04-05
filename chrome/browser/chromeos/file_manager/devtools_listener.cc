// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/devtools_listener.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "url/url_util.h"

namespace file_manager {

namespace {

base::StringPiece SpanToStringPiece(const base::span<const uint8_t>& s) {
  return {reinterpret_cast<const char*>(s.data()), s.size()};
}

std::string EncodeURIComponent(const std::string& component) {
  url::RawCanonOutputT<char> encoded;
  url::EncodeURIComponent(component.c_str(), component.size(), &encoded);
  return {encoded.data(), encoded.length()};
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
                                         const std::string& prefix) {
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

void DevToolsListener::SetupCoverageStore(const base::FilePath& store_path) {
  if (!base::PathExists(store_path))
    CHECK(base::CreateDirectory(store_path));

  base::FilePath tests = store_path.AppendASCII("tests");
  if (!base::PathExists(tests))
    CHECK(base::CreateDirectory(tests));

  base::FilePath scripts = store_path.AppendASCII("scripts");
  if (!base::PathExists(scripts))
    CHECK(base::CreateDirectory(scripts));
}

void DevToolsListener::Start(content::DevToolsAgentHost* host) {
  std::string enable_runtime = "{\"id\":10,\"method\":\"Runtime.enable\"}";
  SendCommandMessage(host, enable_runtime);

  std::string enable_page = "{\"id\":11,\"method\":\"Page.enable\"}";
  SendCommandMessage(host, enable_page);
}

bool DevToolsListener::StartJSCoverage(content::DevToolsAgentHost* host) {
  std::string enable_profiler = "{\"id\":20,\"method\":\"Profiler.enable\"}";
  SendCommandMessage(host, enable_profiler);

  std::string start_precise_coverage =
      "{\"id\":21,\"method\":\"Profiler.startPreciseCoverage\",\"params\":{"
      "\"callCount\":true,\"detailed\":true}}";
  SendCommandMessage(host, start_precise_coverage);

  std::string enable_debugger = "{\"id\":22,\"method\":\"Debugger.enable\"}";
  SendCommandMessage(host, enable_debugger);

  std::string skip_all_pauses =
      "{\"id\":23,\"method\":\"Debugger.setSkipAllPauses\""
      ",\"params\":{\"skip\":true}}";
  SendCommandMessage(host, skip_all_pauses);

  return true;
}

void DevToolsListener::StopAndStoreJSCoverage(content::DevToolsAgentHost* host,
                                              const base::FilePath& store,
                                              const std::string& test) {
  std::string get_precise_coverage =
      "{\"id\":40,\"method\":\"Profiler.takePreciseCoverage\"}";
  SendCommandMessage(host, get_precise_coverage);
  AwaitCommandResponse(40);

  script_coverage_ = std::move(value_);
  StoreScripts(host, store);

  std::string stop_debugger = "{\"id\":41,\"method\":\"Debugger.disable\"}";
  SendCommandMessage(host, stop_debugger);

  std::string stop_profiler = "{\"id\":42,\"method\":\"Profiler.disable\"}";
  SendCommandMessage(host, stop_profiler);

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
  CHECK(result->SetString("encodedHostURL", EncodeURIComponent(url)));
  CHECK(result->SetString("hostTitle", host->GetTitle()));
  CHECK(result->SetString("hostType", host->GetType()));
  CHECK(result->SetString("hostTest", test));
  CHECK(result->SetString("hostURL", url));

  const std::string md5 = base::MD5String(HostString(host, test));
  std::string coverage = base::StrCat({test, ".", md5, uuid_, ".cov.json"});
  base::FilePath path = store.AppendASCII("tests").Append(coverage);

  CHECK(result->SetList("result", std::move(entries)));
  CHECK(base::JSONWriter::Write(*result, &coverage));
  base::WriteFile(path, coverage.data(), coverage.size());

  script_coverage_.reset();
  script_hash_map_.clear();
  script_id_map_.clear();
  script_.clear();

  AwaitCommandResponse(42);
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

    std::string get_script_source = base::StringPrintf(
        "{\"id\":50,\"method\":\"Debugger.getScriptSource\""
        ",\"params\":{\"scriptId\":\"%s\"}}",
        id.c_str());
    SendCommandMessage(host, get_script_source);
    AwaitCommandResponse(50);

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
    CHECK(script->SetString("encodedURL", EncodeURIComponent(url)));
    CHECK(script->SetString("hash", hash));
    CHECK(script->SetString("text", text));
    CHECK(script->SetString("url", url));

    base::FilePath path =
        store.AppendASCII("scripts").Append(hash.append(".js.json"));
    CHECK(base::JSONWriter::Write(*script, &text));
    if (!base::PathExists(path))  // script de-duplication
      base::WriteFile(path, text.data(), text.size());
  }
}

void DevToolsListener::SendCommandMessage(content::DevToolsAgentHost* host,
                                          const std::string& command) {
  auto message = base::as_bytes(base::make_span(command));
  host->DispatchProtocolMessage(this, message);
}

void DevToolsListener::AwaitCommandResponse(int id) {
  value_.reset();
  value_id_ = id;

  base::RunLoop run_loop;
  value_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void DevToolsListener::DispatchProtocolMessage(
    content::DevToolsAgentHost* host,
    base::span<const uint8_t> message) {
  if (!navigated_)
    return;

  if (VLOG_IS_ON(2))
    VLOG(2) << SpanToStringPiece(message);

  std::unique_ptr<base::DictionaryValue> value = base::DictionaryValue::From(
      base::JSONReader::ReadDeprecated(SpanToStringPiece(message)));
  CHECK(value);

  std::string* method = value->FindStringPath("method");
  if (method) {
    if (*method == "Runtime.executionContextsCreated")
      script_.clear();
    else if (*method == "Debugger.scriptParsed")
      script_.push_back(std::move(value));
    return;
  }

  base::Optional<int> id = value->FindIntPath("id");
  if (id.has_value() && id.value() == value_id_) {
    value_ = std::move(value);
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

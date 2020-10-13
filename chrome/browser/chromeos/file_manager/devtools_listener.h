// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_DEVTOOLS_LISTENER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_DEVTOOLS_LISTENER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"

namespace file_manager {

// Collects code coverage from a WebContents during a
// browser test using Chrome Devtools Protocol (CDP).
class DevToolsListener : public content::DevToolsAgentHostClient {
 public:
  // Attaches to a host and enables CDP.
  DevToolsListener(content::DevToolsAgentHost* host, uint32_t uuid);
  ~DevToolsListener() override;

  // Starts code coverage.
  void Navigated(content::DevToolsAgentHost* host);

  // Returns true if host has started code coverage.
  bool HasCoverage(content::DevToolsAgentHost* host);

  // If host HasCoverage() collect the coverage and
  // write it into the |store|.
  void GetCoverage(content::DevToolsAgentHost* host,
                   const base::FilePath& store,
                   const std::string& test);

  // Detaches from a host.
  void Detach(content::DevToolsAgentHost* host);

  // Returns a string that uniquely identifies a host
  // with an optional prefix.
  static std::string HostString(content::DevToolsAgentHost* host,
                                const std::string& prefix);

 private:
  // Enable CDP on host.
  void Start(content::DevToolsAgentHost* host);

  // Starts JavaScript code coverage on host.
  bool StartJSCoverage(content::DevToolsAgentHost* host);

  // Collects JavaScript code coverage on host and writes
  // it into the |store|.
  void StopAndStoreJSCoverage(content::DevToolsAgentHost* host,
                              const base::FilePath& store,
                              const std::string& test);

  // Stores scripts that are parsed during execution on host.
  void StoreScripts(content::DevToolsAgentHost* host,
                    const base::FilePath& store);

  // Await CDP response to command |id|.
  void AwaitMessageResponse(int id);

  // Receives CDP messages sent by host.
  void DispatchProtocolMessage(content::DevToolsAgentHost* host,
                               base::span<const uint8_t> span_message) override;

  // Returns true if URL should be attached to.
  bool MayAttachToURL(const GURL& url, bool is_webui) override;

  // Clean up when host is closed.
  void AgentHostClosed(content::DevToolsAgentHost* host) override;

 private:
  std::vector<std::unique_ptr<base::DictionaryValue>> script_;
  std::unique_ptr<base::DictionaryValue> script_coverage_;
  std::map<std::string, std::string> script_hash_map_;
  std::map<std::string, std::string> script_id_map_;

  base::OnceClosure value_closure_;
  std::unique_ptr<base::DictionaryValue> value_;
  int value_id_;

  const std::string uuid_;
  bool navigated_ = false;
  bool attached_ = true;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_DEVTOOLS_LISTENER_H_

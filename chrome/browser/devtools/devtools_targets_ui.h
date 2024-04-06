// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGETS_UI_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGETS_UI_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"

class Profile;

class DevToolsTargetsUIHandler {
 public:
  using Callback =
      base::RepeatingCallback<void(const std::string&, const base::Value&)>;

  DevToolsTargetsUIHandler(const std::string& source_id, Callback callback);

  DevToolsTargetsUIHandler(const DevToolsTargetsUIHandler&) = delete;
  DevToolsTargetsUIHandler& operator=(const DevToolsTargetsUIHandler&) = delete;

  virtual ~DevToolsTargetsUIHandler();

  std::string source_id() const { return source_id_; }

  static std::unique_ptr<DevToolsTargetsUIHandler> CreateForLocal(
      Callback callback,
      Profile* profile);

  static std::unique_ptr<DevToolsTargetsUIHandler> CreateForAdb(
      Callback callback,
      Profile* profile);

  scoped_refptr<content::DevToolsAgentHost> GetTarget(
      const std::string& target_id);

  virtual void Open(const std::string& browser_id, const std::string& url);

  virtual scoped_refptr<content::DevToolsAgentHost> GetBrowserAgentHost(
      const std::string& browser_id);

  virtual void ForceUpdate();

 protected:
  base::Value::Dict Serialize(content::DevToolsAgentHost* host);
  void SendSerializedTargets(const base::Value& list);

  using TargetMap =
      std::map<std::string, scoped_refptr<content::DevToolsAgentHost>>;
  TargetMap targets_;

 private:
  const std::string source_id_;
  Callback callback_;
};

class PortForwardingStatusSerializer
    : private DevToolsAndroidBridge::PortForwardingListener {
 public:
  using Callback = base::RepeatingCallback<void(base::Value)>;

  PortForwardingStatusSerializer(const Callback& callback, Profile* profile);
  ~PortForwardingStatusSerializer() override;

  void PortStatusChanged(const ForwardingStatus& status) override;

 private:
  Callback callback_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGETS_UI_H_

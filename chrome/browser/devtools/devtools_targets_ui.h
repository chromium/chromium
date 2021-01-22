// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGETS_UI_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGETS_UI_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"

namespace base {
class ListValue;
class DictionaryValue;
}

class Profile;

class DevToolsTargetsUIHandler {
 public:
  using Callback =
      base::RepeatingCallback<void(const std::string&, const base::ListValue&)>;

  DevToolsTargetsUIHandler(const std::string& source_id, Callback callback);
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
  std::unique_ptr<base::DictionaryValue> Serialize(
      content::DevToolsAgentHost* host);
  void SendSerializedTargets(const base::ListValue& list);

  using TargetMap =
      std::map<std::string, scoped_refptr<content::DevToolsAgentHost>>;
  TargetMap targets_;

 private:
  const std::string source_id_;
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsTargetsUIHandler);
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
  Profile* profile_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGETS_UI_H_

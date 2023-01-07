// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_MOCK_PLUGIN_VM_MANAGER_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_MOCK_PLUGIN_VM_MANAGER_H_

#include "base/functional/callback.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace plugin_vm {
namespace test {

class MockPluginVmManager : public PluginVmManager {
 public:
  MockPluginVmManager();
  ~MockPluginVmManager() override;
  MockPluginVmManager(const MockPluginVmManager&) = delete;
  MockPluginVmManager& operator=(const MockPluginVmManager&) = delete;

  MOCK_METHOD(void, OnPrimaryUserSessionStarted, (), ());
  MOCK_METHOD(void, LaunchPluginVm, (LaunchPluginVmCallback callback), ());
  MOCK_METHOD(void, RelaunchPluginVm, (), ());
  MOCK_METHOD(void, StopPluginVm, (const std::string& name, bool force), ());
  MOCK_METHOD(void, UninstallPluginVm, (), ());
  MOCK_METHOD(uint64_t, seneschal_server_handle, (), (const));
  MOCK_METHOD(void,
              StartDispatcher,
              (base::OnceCallback<void(bool success)> callback),
              (const));
  MOCK_METHOD(void,
              AddVmStartingObserver,
              (ash::VmStartingObserver * observer),
              ());
  MOCK_METHOD(void,
              RemoveVmStartingObserver,
              (ash::VmStartingObserver * observer),
              ());
  MOCK_METHOD(vm_tools::plugin_dispatcher::VmState, vm_state, (), (const));
  MOCK_METHOD(bool, IsRelaunchNeededForNewPermissions, (), (const));
};

}  // namespace test
}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_MOCK_PLUGIN_VM_MANAGER_H_

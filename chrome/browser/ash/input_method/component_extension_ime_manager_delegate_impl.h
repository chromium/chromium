// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_IMPL_H_

#include <optional>
#include <set>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"
#include "ui/base/ime/ash/component_extension_ime_manager_delegate.h"

class Profile;

namespace ash {
namespace input_method {

// The implementation class of ComponentExtensionIMEManagerDelegate.
class ComponentExtensionIMEManagerDelegateImpl
    : public ComponentExtensionIMEManagerDelegate {
 public:
  ComponentExtensionIMEManagerDelegateImpl();

  ComponentExtensionIMEManagerDelegateImpl(
      const ComponentExtensionIMEManagerDelegateImpl&) = delete;
  ComponentExtensionIMEManagerDelegateImpl& operator=(
      const ComponentExtensionIMEManagerDelegateImpl&) = delete;

  ~ComponentExtensionIMEManagerDelegateImpl() override;

  // ComponentExtensionIMEManagerDelegate overrides:
  std::vector<ComponentExtensionIME> ListIME() override;
  void Load(Profile* profile,
            const std::string& extension_id,
            const std::string& manifest,
            const base::FilePath& file_path) override;
  bool IsInLoginLayoutAllowlist(const std::string& layout) override;

  static bool IsIMEExtensionID(const std::string& id);

 private:
  // Reads component extensions and extract their localized information: name,
  // description and ime id. This function fills them into |out_imes|.
  static void ReadComponentExtensionsInfo(
      std::vector<ComponentExtensionIME>* out_imes);

  // Parses manifest string into dictionary value.
  static std::optional<base::Value::Dict> ParseManifest(
      std::string_view manifest_string);

  // Reads extension information: description, option page. This function
  // returns true on success, otherwise returns false.
  static bool ReadExtensionInfo(const base::Value::Dict& manifest,
                                const std::string& extension_id,
                                ComponentExtensionIME* out);

  // Reads each engine component in |dict|. |dict| is given by GetList with
  // kInputComponents key from manifest. This function returns true on success,
  // otherwise return false. This function must be called on FILE thread.
  static bool ReadEngineComponent(
      const ComponentExtensionIME& component_extension,
      const base::Value::Dict& dict,
      ComponentExtensionEngine* out);

  // The list of component extension IME.
  std::vector<ComponentExtensionIME> component_extension_list_;

  std::set<std::string> login_layout_set_;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_IMPL_H_

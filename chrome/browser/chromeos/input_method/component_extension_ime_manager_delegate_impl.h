// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_IMPL_H_

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager_delegate.h"

class Profile;

namespace chromeos {

// The implementation class of ComponentExtensionIMEManagerDelegate.
class ComponentExtensionIMEManagerDelegateImpl
    : public ComponentExtensionIMEManagerDelegate {
 public:
  ComponentExtensionIMEManagerDelegateImpl();
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

  // Parses manifest string to manifest json dictionary value.
  static std::unique_ptr<base::DictionaryValue> GetManifest(
      const std::string& manifest_string);

  // Reads extension information: description, option page. This function
  // returns true on success, otherwise returns false.
  static bool ReadExtensionInfo(const base::DictionaryValue& manifest,
                                const std::string& extension_id,
                                ComponentExtensionIME* out);

  // Reads each engine component in |dict|. |dict| is given by GetList with
  // kInputComponents key from manifest. This function returns true on success,
  // otherwise return false. This function must be called on FILE thread.
  static bool ReadEngineComponent(
      const ComponentExtensionIME& component_extension,
      const base::DictionaryValue& dict,
      ComponentExtensionEngine* out);

  // The list of component extension IME.
  std::vector<ComponentExtensionIME> component_extension_list_;

  std::set<std::string> login_layout_set_;

  DISALLOW_COPY_AND_ASSIGN(ComponentExtensionIMEManagerDelegateImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_COMPONENT_EXTENSION_IME_MANAGER_DELEGATE_IMPL_H_

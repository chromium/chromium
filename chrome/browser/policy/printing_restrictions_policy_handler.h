// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_PRINTING_RESTRICTIONS_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_PRINTING_RESTRICTIONS_POLICY_HANDLER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "printing/backend/printing_restrictions.h"
#include "printing/buildflags/buildflags.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

template <class Mode>
class PrintingEnumPolicyHandler : public TypeCheckingPolicyHandler {
  static_assert(std::is_enum<Mode>::value,
                "PrintingEnumPolicyHandler can only be used with enums");

 public:
  PrintingEnumPolicyHandler(
      const char* policy_name,
      const char* pref_name,
      const base::flat_map<std::string, Mode>& policy_value_to_mode);
  ~PrintingEnumPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  bool GetValue(const PolicyMap& policies,
                PolicyErrorMap* errors,
                Mode* result);

  const char* const policy_name_;
  const char* const pref_name_;
  base::flat_map<std::string, Mode> policy_value_to_mode_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
class PrintingAllowedColorModesPolicyHandler
    : public PrintingEnumPolicyHandler<printing::ColorModeRestriction> {
 public:
  PrintingAllowedColorModesPolicyHandler();
  ~PrintingAllowedColorModesPolicyHandler() override;
};

class PrintingColorDefaultPolicyHandler
    : public PrintingEnumPolicyHandler<printing::ColorModeRestriction> {
 public:
  PrintingColorDefaultPolicyHandler();
  ~PrintingColorDefaultPolicyHandler() override;
};

class PrintingAllowedDuplexModesPolicyHandler
    : public PrintingEnumPolicyHandler<printing::DuplexModeRestriction> {
 public:
  PrintingAllowedDuplexModesPolicyHandler();
  ~PrintingAllowedDuplexModesPolicyHandler() override;
};

class PrintingDuplexDefaultPolicyHandler
    : public PrintingEnumPolicyHandler<printing::DuplexModeRestriction> {
 public:
  PrintingDuplexDefaultPolicyHandler();
  ~PrintingDuplexDefaultPolicyHandler() override;
};

class PrintingAllowedPinModesPolicyHandler
    : public PrintingEnumPolicyHandler<printing::PinModeRestriction> {
 public:
  PrintingAllowedPinModesPolicyHandler();
  ~PrintingAllowedPinModesPolicyHandler() override;
};

class PrintingPinDefaultPolicyHandler
    : public PrintingEnumPolicyHandler<printing::PinModeRestriction> {
 public:
  PrintingPinDefaultPolicyHandler();
  ~PrintingPinDefaultPolicyHandler() override;
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
class PrintingAllowedBackgroundGraphicsModesPolicyHandler
    : public PrintingEnumPolicyHandler<
          printing::BackgroundGraphicsModeRestriction> {
 public:
  PrintingAllowedBackgroundGraphicsModesPolicyHandler();
  ~PrintingAllowedBackgroundGraphicsModesPolicyHandler() override;
};

class PrintingBackgroundGraphicsDefaultPolicyHandler
    : public PrintingEnumPolicyHandler<
          printing::BackgroundGraphicsModeRestriction> {
 public:
  PrintingBackgroundGraphicsDefaultPolicyHandler();
  ~PrintingBackgroundGraphicsDefaultPolicyHandler() override;
};

class PrintingPaperSizeDefaultPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  PrintingPaperSizeDefaultPolicyHandler();
  ~PrintingPaperSizeDefaultPolicyHandler() override;

  // ConfigurationPolicyHandler implementation:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  bool CheckIntSubkey(const base::Value::Dict& dict,
                      const std::string& key,
                      PolicyErrorMap* errors);
  bool GetValue(const PolicyMap& policies,
                PolicyErrorMap* errors,
                const base::Value** result);
};

class PrintPdfAsImageDefaultPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  PrintPdfAsImageDefaultPolicyHandler();
  ~PrintPdfAsImageDefaultPolicyHandler() override;

  // ConfigurationPolicyHandler implementation:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_PRINTING_RESTRICTIONS_POLICY_HANDLER_H_

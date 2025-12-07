// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_DELEGATE_IMPL_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "ui/base/ime/ash/input_method_delegate.h"

class PrefService;

namespace ash {
namespace input_method {

// Accesses the hardware keyboard layout and application locale from the
// BrowserProcess.
class InputMethodDelegateImpl : public InputMethodDelegate {
 public:
  // `local_state` must be non-null, and must outlive `this`.
  explicit InputMethodDelegateImpl(PrefService* local_state);

  InputMethodDelegateImpl(const InputMethodDelegateImpl&) = delete;
  InputMethodDelegateImpl& operator=(const InputMethodDelegateImpl&) = delete;

  ~InputMethodDelegateImpl() override;

  // InputMethodDelegate implementation.
  std::string GetHardwareKeyboardLayouts() const override;
  std::u16string GetLocalizedString(int resource_id) const override;
  void SetHardwareKeyboardLayoutForTesting(const std::string& layout) override;

 private:
  const raw_ref<PrefService> local_state_;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_DELEGATE_IMPL_H_

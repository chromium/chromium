// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "extensions/common/extension_id.h"

class GlobalConfirmInfoBar;

namespace extensions {

// An infobar used to globally warn users that an extension is debugging the
// browser (which has security consequences).
class ExtensionDevToolsInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static constexpr base::TimeDelta kAutoCloseDelay = base::Seconds(5);
  using CallbackList = base::OnceClosureList;

  // Ensures a global infobar corresponding to the supplied extension is
  // showing and registers |destroyed_callback| with it to be called back on
  // destruction.
  static base::CallbackListSubscription Create(
      const ExtensionId& extension_id,
      const std::string& extension_name,
      base::OnceClosure destroyed_callback);

  ExtensionDevToolsInfoBarDelegate(const ExtensionDevToolsInfoBarDelegate&) =
      delete;
  ExtensionDevToolsInfoBarDelegate& operator=(
      const ExtensionDevToolsInfoBarDelegate&) = delete;
  ~ExtensionDevToolsInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  std::u16string GetMessageText() const override;
  gfx::ElideBehavior GetMessageElideBehavior() const override;
  int GetButtons() const override;

 private:
  ExtensionDevToolsInfoBarDelegate(ExtensionId extension_id,
                                   const std::string& extension_name);

  // Adds |destroyed_callback| to the list of callbacks to run on destruction.
  base::CallbackListSubscription RegisterDestroyedCallback(
      base::OnceClosure destroyed_callback);

  // Autocloses the infobar_ after 5 seconds, only when no callbacks remain.
  void MaybeStartAutocloseTimer();

  const ExtensionId extension_id_;
  const std::u16string extension_name_;
  // infobar_ is set after attaching an extension and is deleted 5 seconds after
  // detaching the extension. |infobar_| owns this object and is therefore
  // guaranteed to outlive it.
  raw_ptr<GlobalConfirmInfoBar> infobar_ = nullptr;
  CallbackList callback_list_;
  base::OneShotTimer timer_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE_H_

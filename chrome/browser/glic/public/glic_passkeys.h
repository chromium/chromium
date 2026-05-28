// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_PASSKEYS_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_PASSKEYS_H_

#include "base/types/pass_key.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_context_menu_invocation_helper.h"

class GlicExperimentalTriggeringMessageHandler;
namespace tabs {
class TabInterface;
}

namespace extensions {
class GlicPrivateInvokeFunction;
class PdfViewerPrivateGlicSummarizeFunction;
}

namespace indigo {
class IndigoPageActionController;
}

class PasswordChangeFromCheckupDelegate;

namespace ttc {
class AiOverlayTools;
}

namespace skills {
class SkillsUiTabController;
class SkillsInvocationInteractiveUiTest;
}  // namespace skills

namespace glic {

class AnchoredNudgeController;
class GlicInternalsPageHandler;

template <mojom::InvocationSource Source>
class AndroidAutoSubmitPasskeyHelper;

// Passkey for invoking glic with auto submit. Reach out to OWNERS before
// adding new callers.
class InvokeWithAutoSubmitPasskeyProvider {
 public:
  using PassKey = base::PassKey<InvokeWithAutoSubmitPasskeyProvider>;

 private:
  static PassKey GetPassKey() { return PassKey(); }

  // Example of how to add new friends:
  // friend class SomeClassThatNeedsAutoSubmit;
  // friend void SomeClass::SomeFunctionThatNeedsAutoSubmit();
  friend class AnchoredNudgeController;
  friend void GlicContextMenuInvocationHelper::HandleContextualMenuClick(
      tabs::TabInterface* tab);
  friend class extensions::GlicPrivateInvokeFunction;
  friend class extensions::PdfViewerPrivateGlicSummarizeFunction;
  friend class ::PasswordChangeFromCheckupDelegate;
  friend class GlicInternalsPageHandler;
  friend class GlicInstanceCoordinatorBrowserTest;
  friend class GlicInvokeBrowserTest;
  friend class GlicInstanceCoordinatorTrustFirstOnboardingArm1BrowserTest;
  friend class GlicApiTestPasskeys;
  friend class ::GlicExperimentalTriggeringMessageHandler;
  friend class GlicCueTarget;
  friend class ::indigo::IndigoPageActionController;
  friend class ::ttc::AiOverlayTools;
  friend class ::skills::SkillsUiTabController;
  friend class ::skills::SkillsInvocationInteractiveUiTest;
  friend class AndroidAutoSubmitPasskeyHelper<
      mojom::InvocationSource::kUniversalCart>;
};

using InvokeWithAutoSubmitPasskey =
    base::PassKey<InvokeWithAutoSubmitPasskeyProvider>;

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_PASSKEYS_H_

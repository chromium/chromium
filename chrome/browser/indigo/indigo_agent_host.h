// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_AGENT_HOST_H_
#define CHROME_BROWSER_INDIGO_INDIGO_AGENT_HOST_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/common/indigo/indigo.mojom.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_user_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

class GURL;

namespace indigo {

// Browser-side host for the IndigoAgent in the renderer. This class is
// page-scoped, meaning it is correctly managed and cleaned up when a navigation
// occurs.
class IndigoAgentHost : public content::PageUserData<IndigoAgentHost>,
                        public chrome::mojom::IndigoAgentHost {
 public:
  ~IndigoAgentHost() override;

  // Invokes the Indigo feature on the page. If the content script hasn't been
  // injected yet for this page, it will be injected automatically.
  //
  // Returns true if the invocation is being handled (either started or queued).
  // Returns false if no script is configured via the command line.
  bool Invoke();

  // chrome::mojom::IndigoAgentHost:
  void StartImageReplacement(
      mojo::PendingRemote<blink::mojom::ImageReplacement> replacement,
      bool is_primary,
      StartImageReplacementCallback callback) override;

 private:
  enum class InjectionState {
    kNotInjected,
    kInjecting,
    kInjected,
  };

  explicit IndigoAgentHost(content::Page& page);
  friend class content::PageUserData<IndigoAgentHost>;

  void OnScriptLoaded(const GURL& script_url,
                      std::optional<std::string> script_content);

  chrome::mojom::IndigoAgent& GetAgent();

  mojo::AssociatedReceiver<chrome::mojom::IndigoAgentHost> receiver_{this};
  mojo::AssociatedRemote<chrome::mojom::IndigoAgent> agent_;
  InjectionState injection_state_ = InjectionState::kNotInjected;

  // Number of times Invoke() was called while injection was in progress.
  int pending_invoke_count_ = 0;

  PAGE_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<IndigoAgentHost> weak_factory_{this};
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_AGENT_HOST_H_

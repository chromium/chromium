// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_H_

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/service/glic_conversation_helper.h"
#include "chrome/browser/glic/service/glic_instance_delegate.h"

class BrowserWindowInterface;
class Profile;

namespace tabs {
class TabInterface;
}

namespace glic {

class GlicUiEmbedder;

// A GlicInstance owns a single host keeping any state that must exist for the
// lifetime of the host. When a host is showing, the GlicInstance creates a
// GlicUiEmbedder to display the webcontents in. An instance (and host) exist
// even if it has no GlicUiEmbedder showing the UI. A host could have many
// different GlicUiEmbedders during its lifetime.
class GlicInstance : public GlicInstanceDelegate {
 public:
  enum class EmbedderType {
    kSidePanel,
    kFloating,
  };

  class AttachmentDelegate {
   public:
    virtual ~AttachmentDelegate() = default;
    virtual void AttachInstance(GlicInstance* instance) = 0;
    virtual void DetachInstance(GlicInstance* instance) = 0;
    virtual void OnInstanceOrphaned(GlicInstance* instance) = 0;
  };

  GlicInstance(Profile* profile,
               std::unique_ptr<Host> host,
               ConversationId conversation_id,
               base::WeakPtr<AttachmentDelegate> attachment_delegate);
  ~GlicInstance() override;

  GlicInstance(const GlicInstance&) = delete;
  GlicInstance& operator=(const GlicInstance&) = delete;

  Profile* profile() { return profile_; }
  Host& host() { return *host_; }
  GlicUiEmbedder& embedder();

  void DisassociateWindow();

  void AttachInstance() override;
  void DetachInstance() override;
  bool IsShowing() const;
  BrowserWindowInterface* associated_bwi() const { return associated_bwi_; }
  const ConversationId& conversation_id() const { return conversation_id_; }

  // These methods should only be called by the GlicInstanceCoordinator.
  EmbedderType GetEmbedderType();
  void SetEmbedderType(EmbedderType type);
  void Show(tabs::TabInterface* tab);
  void Close();
  void Toggle();

  // Manages the association of this conversation with a tab.
  void AssociateWithTab(tabs::TabInterface* tab);
  void DisassociateFromTab(tabs::TabInterface* tab);
  bool IsOrphaned() const;

  // InstanceDelegate:
  void CloseInstanceAndShutdown() override;
  void CreateTab() override;
  void CreateTask() override;
  void PerformActions() override;
  void StopActorTask() override;
  void PauseActorTask() override;
  void ResumeActorTask() override;
  void GetZeroStateSuggestionsAndSubscribe() override;
  void GetZeroStateSuggestionsForFocusedTab() override;

 private:
  void OnAssociatedTabDestroyed(tabs::TabInterface* tab,
                                const ConversationId& conversation_id);

  raw_ptr<Profile> profile_;

  // Replaces GlicWindowController on existing GlicKeyedService.
  std::unique_ptr<GlicUiEmbedder> embedder_;
  EmbedderType embedder_type_ = EmbedderType::kSidePanel;

  // The browser window this instance is associated with. This persists even
  // when detached.
  raw_ptr<BrowserWindowInterface> associated_bwi_ = nullptr;
  base::WeakPtr<AttachmentDelegate> attachment_delegate_;
  const ConversationId conversation_id_;

  base::flat_map<tabs::TabInterface*, base::CallbackListSubscription>
      associated_tab_subscriptions_;
  std::unique_ptr<Host> host_;
  base::WeakPtrFactory<GlicInstance> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_H_

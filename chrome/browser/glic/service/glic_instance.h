// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_H_

#include <variant>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/service/glic_conversation_helper.h"
#include "chrome/browser/glic/service/glic_instance_delegate.h"

class BrowserWindowInterface;
class Profile;

namespace views {
class View;
}

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

  void DisassociateWindow();

  void AttachInstance() override;
  void DetachInstance() override;
  bool IsShowing() const;
  BrowserWindowInterface* associated_bwi() const { return associated_bwi_; }
  const ConversationId& conversation_id() const { return conversation_id_; }

  // These methods should only be called by the GlicInstanceCoordinator.
  void Show(EmbedderType type, tabs::TabInterface* tab);
  void Close();
  void Toggle(EmbedderType type, tabs::TabInterface* tab);
  std::unique_ptr<views::View> CreateViewForSidePanel(tabs::TabInterface* tab);

  // Manages the association of this conversation with a tab.
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
  // A tag type to represent the floating embedder key.
  struct FloatingEmbedderKey {
    auto operator<=>(const FloatingEmbedderKey&) const = default;
  };

  using EmbedderKey = std::variant<FloatingEmbedderKey, tabs::TabInterface*>;

  struct EmbedderEntry {
    EmbedderEntry();
    ~EmbedderEntry();
    EmbedderEntry(EmbedderEntry&&);
    EmbedderEntry& operator=(EmbedderEntry&&);

    std::unique_ptr<GlicUiEmbedder> embedder;
    base::CallbackListSubscription destruction_subscription;
  };

  EmbedderKey GetEmbedderKey(EmbedderType type, tabs::TabInterface* tab);
  GlicUiEmbedder* GetActiveEmbedder();
  GlicUiEmbedder* GetEmbedderForTab(tabs::TabInterface* tab);
  void DeactivateCurrentEmbedder();
  GlicUiEmbedder* CreateActiveEmbedderFor(const EmbedderKey& key);
  void MaybeShowHostUi(GlicUiEmbedder* embedder);
  void OnAssociatedTabDestroyed(tabs::TabInterface* tab,
                                const ConversationId& conversation_id);

  raw_ptr<Profile> profile_;

  // The browser window this instance is associated with. This persists even
  // when detached.
  raw_ptr<BrowserWindowInterface> associated_bwi_ = nullptr;
  base::WeakPtr<AttachmentDelegate> attachment_delegate_;
  const ConversationId conversation_id_;

  // The single source of truth for all embedders.
  // A tabs::TabInterface* key is a tab-bound side panel.
  // A FloatingEmbedderKey key is the instance-bound floating panel.
  base::flat_map<EmbedderKey, EmbedderEntry> embedders_;

  // The single, unambiguous source of truth for the active UI.
  std::optional<EmbedderKey> active_embedder_key_;

  std::unique_ptr<Host> host_;
  base::WeakPtrFactory<GlicInstance> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_H_

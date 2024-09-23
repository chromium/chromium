// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DESK_TEMPLATE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DESK_TEMPLATE_ASH_H_

#include <list>

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/desk_template.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "url/gurl.h"

namespace crosapi {

// Implements the crosapi interface for desk template. Lives in Ash-Chrome
// on the UI thread.
class DeskTemplateAsh : public mojom::DeskTemplate {
 public:
  DeskTemplateAsh();
  DeskTemplateAsh(const DeskTemplateAsh&) = delete;
  DeskTemplateAsh& operator=(const DeskTemplateAsh&) = delete;
  ~DeskTemplateAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::DeskTemplate> receiver);

  // Called by ash's internal desk template implementation.
  // Forwarded to Lacros.
  void GetBrowserInformation(
      const std::string& window_unique_id,
      base::OnceCallback<void(crosapi::mojom::DeskTemplateStatePtr)> callback);
  void CreateBrowserWithRestoredData(
      const gfx::Rect& bounds,
      const ui::mojom::WindowShowState show_state,
      crosapi::mojom::DeskTemplateStatePtr additional_state);
  void GetFaviconImage(
      const GURL& url,
      uint64_t lacros_profile_id,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback);

  // crosapi::mojom::DeskTemplate:
  void AddDeskTemplateClient(
      mojo::PendingRemote<mojom::DeskTemplateClient> client) override;

 private:
  // State for the call for desk template data from Ash.
  // The call is replicated for all existing remotes, and no more than the only
  // one of them will return data.
  struct Call {
    Call(uint32_t serial,
         const std::string& window_unique_id,
         uint32_t remote_count,
         base::OnceCallback<void(crosapi::mojom::DeskTemplateStatePtr)>
             callback);
    ~Call();

    // Serial number of this call.
    uint32_t serial;
    // Unique ID of the window this call is made for.
    std::string window_unique_id;
    // How many remotes existed at the moment when the call was registered.
    uint32_t remote_count;
    // The receiver for the data.
    base::OnceCallback<void(crosapi::mojom::DeskTemplateStatePtr)> callback;
  };

  // Receives the response from the single remote.  If the response contains
  // data, forwards it to Ash.
  void OnGetBrowserInformationFromRemote(uint32_t serial,
                                         const std::string& window_unique_id,
                                         mojom::DeskTemplateStatePtr state);

  mojo::ReceiverSet<mojom::DeskTemplate> receivers_;
  // Each separate Lacros process owns its own remote.
  mojo::RemoteSet<mojom::DeskTemplateClient> remotes_;

  // Serial number of the next call.  Incremented every time a call is placed.
  uint32_t serial_ = 0;
  // List of calls scheduled for the remotes.  New calls are pushed to the back.
  std::list<Call> calls_;

  base::WeakPtrFactory<DeskTemplateAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DESK_TEMPLATE_ASH_H_

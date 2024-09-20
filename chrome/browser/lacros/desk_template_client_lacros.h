// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_DESK_TEMPLATE_CLIENT_LACROS_H_
#define CHROME_BROWSER_LACROS_DESK_TEMPLATE_CLIENT_LACROS_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/task/cancelable_task_tracker.h"
#include "chromeos/crosapi/mojom/desk_template.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "url/gurl.h"

class Profile;

// This class gathers desk template data for Ash.
class DeskTemplateClientLacros : public crosapi::mojom::DeskTemplateClient {
 public:
  DeskTemplateClientLacros();
  DeskTemplateClientLacros(const DeskTemplateClientLacros&) = delete;
  DeskTemplateClientLacros& operator=(const DeskTemplateClientLacros&) = delete;
  ~DeskTemplateClientLacros() override;

  // DeskTemplateClient:
  void CreateBrowserWithRestoredData(
      const gfx::Rect& bounds,
      const ui::mojom::WindowShowState show_state,
      crosapi::mojom::DeskTemplateStatePtr additional_state) override;
  void GetBrowserInformation(uint32_t serial,
                             const std::string& window_unique_id,
                             GetBrowserInformationCallback callback) override;
  void GetFaviconImage(const GURL& url,
                       std::optional<uint64_t> profile_id,
                       GetFaviconImageCallback callback) override;

 private:
  // Loads the favicon for the given `url` and `profile`.
  void GetFaviconImageWithProfile(const GURL& url,
                                  GetFaviconImageCallback callback,
                                  Profile* profile);

  // The cancelable task tracker used for retrieving icons from the favicon
  // service.
  base::CancelableTaskTracker task_tracker_;

  mojo::Receiver<crosapi::mojom::DeskTemplateClient> receiver_{this};
};

#endif  // CHROME_BROWSER_LACROS_DESK_TEMPLATE_CLIENT_LACROS_H_

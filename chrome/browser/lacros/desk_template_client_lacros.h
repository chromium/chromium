// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_DESK_TEMPLATE_CLIENT_LACROS_H_
#define CHROME_BROWSER_LACROS_DESK_TEMPLATE_CLIENT_LACROS_H_

#include "chromeos/crosapi/mojom/desk_template.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// This class gathers desk template data for Ash.
class DeskTemplateClientLacros : public crosapi::mojom::DeskTemplateClient {
 public:
  DeskTemplateClientLacros();
  DeskTemplateClientLacros(const DeskTemplateClientLacros&) = delete;
  DeskTemplateClientLacros& operator=(const DeskTemplateClientLacros&) = delete;
  ~DeskTemplateClientLacros() override;

 private:
  // DeskTemplateClient:
  void CreateBrowserWithRestoredData(
      const gfx::Rect& current_bounds,
      const ui::mojom::WindowShowState window_show_state,
      crosapi::mojom::DeskTemplateStatePtr tabstrip_state) override;
  void GetTabStripModelUrls(uint32_t serial,
                            const std::string& window_unique_id,
                            GetTabStripModelUrlsCallback callback) override;

  mojo::Receiver<crosapi::mojom::DeskTemplateClient> receiver_{this};
};

#endif  // CHROME_BROWSER_LACROS_DESK_TEMPLATE_CLIENT_LACROS_H_

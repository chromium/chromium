// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_INTERNALS_CONTEXTUAL_CUEING_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_INTERNALS_CONTEXTUAL_CUEING_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/contextual_cueing/internals/contextual_cueing_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace contextual_cueing_internals {

class ContextualCueingInternalsPageHandler : public mojom::PageHandler {
 public:
  ContextualCueingInternalsPageHandler(
      mojo::PendingReceiver<mojom::PageHandler> receiver,
      Profile* profile);
  ContextualCueingInternalsPageHandler(
      const ContextualCueingInternalsPageHandler&) = delete;
  ContextualCueingInternalsPageHandler& operator=(
      const ContextualCueingInternalsPageHandler&) = delete;
  ~ContextualCueingInternalsPageHandler() override;

  // mojom::PageHandler:
  void GetShownCues(GetShownCuesCallback callback) override;

 private:
  mojo::Receiver<mojom::PageHandler> receiver_;
  raw_ptr<Profile> profile_;
};

}  // namespace contextual_cueing_internals

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_INTERNALS_CONTEXTUAL_CUEING_INTERNALS_PAGE_HANDLER_H_

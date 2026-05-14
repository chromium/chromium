// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_PAGE_HANDLER_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace glic {

class GlicKeyedService;

class GlicExperimentalOptInPageHandler
    : public mojom::ExperimentalOptInPageHandler {
 public:
  GlicExperimentalOptInPageHandler(
      Profile* profile,
      RequiredExperimentalOptIn required_state,
      mojo::PendingReceiver<mojom::ExperimentalOptInPageHandler> receiver);
  GlicExperimentalOptInPageHandler(const GlicExperimentalOptInPageHandler&) =
      delete;
  GlicExperimentalOptInPageHandler& operator=(
      const GlicExperimentalOptInPageHandler&) = delete;
  ~GlicExperimentalOptInPageHandler() override;

  // mojom::ExperimentalOptInPageHandler:
  void Accept() override;
  void Reject() override;

 private:
  GlicKeyedService* GetGlicService();

  mojo::Receiver<mojom::ExperimentalOptInPageHandler> receiver_;
  raw_ptr<Profile> profile_;
  RequiredExperimentalOptIn required_state_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_PAGE_HANDLER_H_

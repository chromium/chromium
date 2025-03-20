// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_coordinator.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace actor {

ActorCoordinator::ActorCoordinator() = default;

ActorCoordinator::~ActorCoordinator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ActorCoordinator::Act(
    tabs::TabInterface* tab,
    const optimization_guide::proto::BrowserAction& action,
    ActionResultCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  content::WebContents& web_contents = *tab->GetContents();

  MayActOnTab(
      *tab,
      base::BindOnce(
          &ActorCoordinator::OnMayActOnTabResponse,
          weak_ptr_factory_.GetWeakPtr(), web_contents.GetWeakPtr(), action,
          web_contents.GetPrimaryMainFrame()->GetLastCommittedOrigin(),
          std::move(callback)));
}

void ActorCoordinator::OnMayActOnTabResponse(
    base::WeakPtr<content::WebContents> web_contents,
    const optimization_guide::proto::BrowserAction& action,
    const url::Origin& evaluated_origin,
    ActionResultCallback callback,
    bool may_act) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!web_contents) {
    std::move(callback).Run(/*succeeded=*/false);
    return;
  }

  tabs::TabInterface* tab =
      tabs::TabInterface::GetFromContents(web_contents.get());
  CHECK(tab);

  if (!evaluated_origin.IsSameOriginWith(
          web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin())) {
    // A cross-origin navigation occurred before we got permission. The result
    // is no longer applicable. For now just fail.
    // TODO(mcnee): Handle this gracefully.
    NOTIMPLEMENTED();
    std::move(callback).Run(/*succeeded=*/false);
    return;
  }

  if (!may_act) {
    std::move(callback).Run(/*succeeded=*/false);
    return;
  }

  // TODO(https://crbug.com/402086021): Use actor tool framework.
  NOTIMPLEMENTED();
  std::move(callback).Run(/*succeeded=*/false);
}

}  // namespace actor

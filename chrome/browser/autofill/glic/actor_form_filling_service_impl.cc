// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/glic/actor_form_filling_service_impl.h"

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"
#include "components/tabs/public/tab_interface.h"

namespace autofill {

ActorFormFillingServiceImpl::~ActorFormFillingServiceImpl() = default;

void ActorFormFillingServiceImpl::GetSuggestions(
    const tabs::TabInterface& tab,
    base::span<const FillRequest> fill_requests,
    base::OnceCallback<void(base::expected<std::vector<ActorFormFillingRequest>,
                                           ActorFormFillingError>)> callback) {}

void ActorFormFillingServiceImpl::FillSuggestions(
    const tabs::TabInterface& tab,
    base::span<const ActorFormFillingSelection> chosen_suggestions,
    base::OnceCallback<void(base::expected<void, ActorFormFillingError>)>
        callback) {}

}  // namespace autofill

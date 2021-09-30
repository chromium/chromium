// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_presenter.h"

#include "ash/public/cpp/desk_template.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "base/bind.h"

namespace ash {

namespace {

// Helper to get the desk model from the shell delegate. May return nullptr.
desks_storage::DeskModel* GetDeskModel() {
  return Shell::Get()->shell_delegate()->GetDeskModel();
}

}  // namespace

DesksTemplatesPresenter::DesksTemplatesPresenter(
    OverviewSession* overview_session)
    : overview_session_(overview_session) {
  DCHECK(overview_session_);

  // TODO(sammiequon): Check if the desk model has loaded yet.
  auto* desk_model = GetDeskModel();
  if (!desk_model)
    return;

  desk_model->GetAllEntries(
      base::BindOnce(&DesksTemplatesPresenter::OnGetAllEntries,
                     weak_ptr_factory_.GetWeakPtr()));
}

DesksTemplatesPresenter::~DesksTemplatesPresenter() = default;

void DesksTemplatesPresenter::OnGetAllEntries(
    desks_storage::DeskModel::GetAllEntriesStatus status,
    std::vector<DeskTemplate*> entries) {
  if (status != desks_storage::DeskModel::GetAllEntriesStatus::kOk)
    return;

  // TODO(sammiequon): Update the UI here.
}

}  // namespace ash

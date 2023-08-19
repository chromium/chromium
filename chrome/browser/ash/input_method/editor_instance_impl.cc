// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_instance_impl.h"

namespace ash {
namespace input_method {
namespace {

std::vector<mojom::PresetTextQueryPtr> GenerateFakeQueries() {
  std::vector<mojom::PresetTextQueryPtr> queries;

  queries.push_back(mojom::PresetTextQuery::New(
      /*text_query_id=*/"1",
      /*name=*/"One",
      /*description=*/"This is query one",
      /*category=*/mojom::TextQueryCategory::kCustom));
  queries.push_back(mojom::PresetTextQuery::New(
      /*text_query_id=*/"2",
      /*name=*/"Two",
      /*description=*/"This is query two",
      /*category=*/mojom::TextQueryCategory::kCustom));
  queries.push_back(mojom::PresetTextQuery::New(
      /*text_query_id=*/"3",
      /*name=*/"Three",
      /*description=*/"This is query three",
      /*category=*/mojom::TextQueryCategory::kCustom));

  return queries;
}

}  // namespace

EditorInstanceImpl::EditorInstanceImpl(Delegate* delegate)
    : delegate_(delegate) {}

EditorInstanceImpl::~EditorInstanceImpl() = default;

void EditorInstanceImpl::GetRewritePresetTextQueries(
    GetRewritePresetTextQueriesCallback callback) {
  std::move(callback).Run(GenerateFakeQueries());
}

void EditorInstanceImpl::CommitEditorResult(
    const std::string& text,
    CommitEditorResultCallback callback) {
  delegate_->CommitEditorResult(text);
  std::move(callback).Run(/*success=*/true);
}

void EditorInstanceImpl::BindReceiver(
    mojo::PendingReceiver<mojom::EditorInstance> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

}  // namespace input_method
}  // namespace ash

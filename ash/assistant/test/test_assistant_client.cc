// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/test/test_assistant_client.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"

namespace ash {

namespace {

constexpr base::TimeDelta kMockCallbackDelayTime =
    base::TimeDelta::FromMilliseconds(250);

std::unique_ptr<ui::AssistantTree> CreateTestAssistantTree() {
  auto tree = std::make_unique<ui::AssistantTree>();
  tree->nodes.emplace_back(std::make_unique<ui::AssistantNode>());
  return tree;
}

}  // namespace

TestAssistantClient::TestAssistantClient() = default;

TestAssistantClient::~TestAssistantClient() = default;

void TestAssistantClient::RequestAssistantStructure(
    RequestAssistantStructureCallback callback) {
  // Pretend to fetch structure asynchronously.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](RequestAssistantStructureCallback callback) {
            std::move(callback).Run(ax::mojom::AssistantExtra::New(),
                                    CreateTestAssistantTree());
          },
          std::move(callback)),
      kMockCallbackDelayTime);
}

}  // namespace ash

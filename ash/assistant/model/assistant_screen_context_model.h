// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_SCREEN_CONTEXT_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_SCREEN_CONTEXT_MODEL_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"

namespace ash {

// A class to cache the Assistant structure. |Future| means that getting the
// structure call will be async and will be returned as soon as the structure is
// set. If the structure has already been set, getting call will be returned
// immediately.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantStructureFuture {
 public:
  using Callback =
      base::OnceCallback<void(const ax::mojom::AssistantStructure&)>;

  AssistantStructureFuture();
  AssistantStructureFuture(const AssistantStructureFuture&) = delete;
  AssistantStructureFuture& operator=(const AssistantStructureFuture&) = delete;
  ~AssistantStructureFuture();

  void SetValue(ax::mojom::AssistantStructurePtr structure);

  void GetValueAsync(Callback callback);

  bool HasValue() const;

  void Clear();

 private:
  void Notify();
  void RunCallback(Callback callback);

  ax::mojom::AssistantStructurePtr structure_;
  std::vector<Callback> callbacks_;
};

class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantScreenContextModel {
 public:
  AssistantScreenContextModel();

  AssistantScreenContextModel(const AssistantScreenContextModel&) = delete;
  AssistantScreenContextModel& operator=(const AssistantScreenContextModel&) =
      delete;

  ~AssistantScreenContextModel();

  void Clear();

  AssistantStructureFuture* assistant_structure() {
    return &assistant_structure_;
  }

 private:
  AssistantStructureFuture assistant_structure_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_SCREEN_CONTEXT_MODEL_H_

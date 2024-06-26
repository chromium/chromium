// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/public/test/fake_optical_character_recognizer.h"

#include "base/task/single_thread_task_runner.h"

namespace screen_ai {

// static
scoped_refptr<screen_ai::OpticalCharacterRecognizer>
FakeOpticalCharacterRecognizer::Create(bool return_empty) {
  return base::MakeRefCounted<screen_ai::FakeOpticalCharacterRecognizer>(
      return_empty);
}

FakeOpticalCharacterRecognizer::FakeOpticalCharacterRecognizer(
    bool return_empty)
    : OpticalCharacterRecognizer(/*profile=*/nullptr,
                                 mojom::OcrClientType::kTest),
      return_empty_(return_empty) {
  ready_ = true;
}

FakeOpticalCharacterRecognizer::~FakeOpticalCharacterRecognizer() = default;

void FakeOpticalCharacterRecognizer::PerformOCR(
    const ::SkBitmap& image,
    base::OnceCallback<void(screen_ai::mojom::VisualAnnotationPtr)> callback) {
  CHECK(return_empty_)
      << "Preset results not defined, please add them here needed.";
  std::move(callback).Run(mojom::VisualAnnotation::New());
}

void FakeOpticalCharacterRecognizer::PerformOCR(
    const SkBitmap& image,
    base::OnceCallback<void(const ui::AXTreeUpdate&)> callback) {
  ui::AXTreeUpdate update;
  if (!return_empty_) {
    update.has_tree_data = true;
    // TODO(nektar): Add a tree ID as well and update tests.
    // update.tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    update.tree_data.title = "Screen AI";
    update.root_id = next_node_id_;
    ui::AXNodeData node;
    node.id = next_node_id_;
    node.role = ax::mojom::Role::kStaticText;
    node.SetNameChecked("Testing");
    node.relative_bounds.bounds = gfx::RectF(1.0f, 2.0f, 1.0f, 2.0f);
    node.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "en-US");
    update.nodes = {node};
    --next_node_id_;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceCallback<void(const ui::AXTreeUpdate&)> callback,
             const ui::AXTreeUpdate update) {
            std::move(callback).Run(update);
          },
          std::move(callback), std::move(update)));
}

void FakeOpticalCharacterRecognizer::FlushForTesting() {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce([](base::RunLoop* run_loop) { run_loop->Quit(); },
                     &run_loop));
  run_loop.Run();
}

}  // namespace screen_ai

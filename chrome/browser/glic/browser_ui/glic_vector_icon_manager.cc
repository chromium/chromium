// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/vector_icon_utils.h"

namespace glic {

class VectorIconData {
 public:
  explicit VectorIconData(
      std::vector<std::vector<gfx::PathElement>> path_elements)
      : path_elements_(std::move(path_elements)) {
    reps_size_ = path_elements_.size();
    reps_ = std::make_unique<gfx::VectorIconRep[]>(reps_size_);
    for (size_t i = 0; i < reps_size_; ++i) {
      reps_[i] = gfx::VectorIconRep{path_elements_[i]};
    }
    icon_ = std::make_unique<gfx::VectorIcon>(reps_.get(), reps_size_, "");
  }

  VectorIconData& operator=(const VectorIconData&) = delete;
  VectorIconData(const VectorIconData&) = delete;

  const gfx::VectorIcon& Icon() const { return *icon_; }

 private:
  // Keep these as members to ensure that the vended icons (and related
  // machinery) remain valid, since they do not own the data to which they
  // refer.
  std::vector<std::vector<gfx::PathElement>> path_elements_;
  std::unique_ptr<gfx::VectorIconRep[]> reps_;
  size_t reps_size_;
  std::unique_ptr<gfx::VectorIcon> icon_;
};

// static
const gfx::VectorIcon& GlicVectorIconManager::GetVectorIcon(int id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Ensure that the storage backing the vector icon persists so that the
  // vended gfx::VectorIcon references will remain valid.
  static base::NoDestructor<
      absl::flat_hash_map<int, std::unique_ptr<VectorIconData>>>
      icon_data;

  auto it = icon_data->find(id);
  if (it != icon_data->end()) {
    return it->second->Icon();
  }

  std::vector<std::vector<gfx::PathElement>> path_elements;
  gfx::ParsePathElements(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(id),
      path_elements);

  auto data = std::make_unique<VectorIconData>(std::move(path_elements));
  CHECK(icon_data->emplace(id, std::move(data)).second);

  return GetVectorIcon(id);
}

}  // namespace glic

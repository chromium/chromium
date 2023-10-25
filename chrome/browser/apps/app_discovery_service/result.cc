// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/result.h"

#include <utility>

namespace apps {

GameExtras* SourceExtras::AsGameExtras() {
  return nullptr;
}

PlayExtras* SourceExtras::AsPlayExtras() {
  return nullptr;
}

Result::Result(AppSource app_source,
               const std::string& icon_id,
               const std::u16string& app_title,
               std::unique_ptr<SourceExtras> source_extras)
    : app_source_(app_source),
      icon_id_(icon_id),
      app_title_(app_title),
      source_extras_(std::move(source_extras)) {}

Result::Result(const Result& other)
    : app_source_(other.app_source_),
      icon_id_(other.icon_id_),
      app_title_(other.app_title_),
      source_extras_(other.source_extras_ ? other.source_extras_->Clone()
                                          : nullptr) {}

Result& Result::operator=(const Result& other) {
  app_source_ = other.app_source_;
  icon_id_ = other.icon_id_;
  app_title_ = other.app_title_;
  source_extras_ =
      other.source_extras_ ? other.source_extras_->Clone() : nullptr;
  return *this;
}

Result::Result(Result&&) = default;

Result& Result::operator=(Result&&) = default;

Result::~Result() = default;

AppSource Result::GetAppSource() const {
  return app_source_;
}

const std::string& Result::GetIconId() const {
  return icon_id_;
}

const std::u16string& Result::GetAppTitle() const {
  return app_title_;
}

SourceExtras* Result::GetSourceExtras() const {
  return source_extras_.get();
}

}  // namespace apps

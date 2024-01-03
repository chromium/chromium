// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_ITEM_H_
#define ASH_BIRCH_BIRCH_ITEM_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "base/files/file_path.h"
#include "base/time/time.h"

namespace ash {

// The base item which is stored by the birch model.
struct ASH_EXPORT BirchItem {
  explicit BirchItem(const std::string& title);
  BirchItem(BirchItem&&) = default;
  BirchItem(const BirchItem&);
  BirchItem& operator=(const BirchItem&);
  ~BirchItem();
  bool operator==(const BirchItem& rhs) const = default;

  const std::string title;
};

// A birch item which contains file path and time information.
struct ASH_EXPORT BirchFileItem : public BirchItem {
  BirchFileItem(const base::FilePath& file_path,
                const std::optional<base::Time>& timestamp);
  BirchFileItem(BirchFileItem&&) = default;
  BirchFileItem(const BirchFileItem&);
  BirchFileItem& operator=(const BirchFileItem&);
  bool operator==(const BirchFileItem& rhs) const = default;
  ~BirchFileItem();

  const base::FilePath file_path;
  const std::optional<base::Time> timestamp;

  // Intended for debugging.
  std::string ToString() const;
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_ITEM_H_

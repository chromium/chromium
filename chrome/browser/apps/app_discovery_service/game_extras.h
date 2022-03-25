// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_GAME_EXTRAS_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_GAME_EXTRAS_H_

#include <string>
#include <vector>

#include "chrome/browser/apps/app_discovery_service/result.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace apps {

class GameExtras : public SourceExtras {
 public:
  // Which cloud gaming source a result comes from. These values are persisted
  // to logs. Entries should not be renumbered or reused.
  enum class Source {
    // TODO(crbug.com/1305880): Replace with real source.
    kTemporarySource = 1,
  };

  GameExtras(const std::string& id,
             const std::u16string& title,
             Source source,
             const absl::optional<std::vector<std::u16string>>& platforms,
             const GURL& icon_url);
  GameExtras(const GameExtras&) = delete;
  GameExtras& operator=(const GameExtras&) = delete;
  ~GameExtras() override;

  const std::string& GetId() const;
  const std::u16string& GetTitle() const;
  Source GetSource() const;
  const absl::optional<std::vector<std::u16string>>& GetPlatforms() const;
  const GURL& GetIconUrl() const;

  // Result::SourceExtras:
  GameExtras* AsGameExtras() override;

 private:
  std::string id_;
  std::u16string title_;
  Source source_;
  absl::optional<std::vector<std::u16string>> platforms_;
  GURL icon_url_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_GAME_EXTRAS_H_

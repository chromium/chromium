// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_GAME_EXTRAS_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_GAME_EXTRAS_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/apps/app_discovery_service/result.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace apps {

class GameExtras : public SourceExtras {
 public:
  GameExtras(const absl::optional<std::vector<std::u16string>>& platforms,
             const std::u16string& source,
             const std::u16string& publisher,
             const base::FilePath& relative_icon_path,
             const GURL& deeplink_url);
  GameExtras(const GameExtras&);
  GameExtras& operator=(const GameExtras&) = delete;
  ~GameExtras() override;

  std::unique_ptr<SourceExtras> Clone() override;

  // Platform(s) that host the game.
  const absl::optional<std::vector<std::u16string>>& GetPlatforms() const;
  // The source from which the game is being pulled from.
  const std::u16string& GetSource() const;
  // The company that published the game.
  const std::u16string& GetPublisher() const;
  const base::FilePath& GetRelativeIconPath() const;
  const GURL& GetDeeplinkUrl() const;

  // Result::SourceExtras:
  GameExtras* AsGameExtras() override;

 private:
  absl::optional<std::vector<std::u16string>> platforms_;
  std::u16string source_;
  std::u16string publisher_;
  base::FilePath relative_icon_path_;
  GURL deeplink_url_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_GAME_EXTRAS_H_

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
  enum class Source {
    // TODO(crbug.com/1305880): Rename to real source once finalized.
    kTestSource,
  };

  GameExtras(const absl::optional<std::vector<std::u16string>>& platforms,
             Source source,
             const GURL& icon_url);
  GameExtras(const GameExtras&);
  GameExtras& operator=(const GameExtras&) = delete;
  ~GameExtras() override;

  std::unique_ptr<SourceExtras> Clone() override;

  const absl::optional<std::vector<std::u16string>>& GetPlatforms() const;
  Source GetSource() const;
  const GURL& GetIconUrl() const;

  // Result::SourceExtras:
  GameExtras* AsGameExtras() override;

 private:
  const absl::optional<std::vector<std::u16string>> platforms_;
  const Source source_;
  const GURL icon_url_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_GAME_EXTRAS_H_

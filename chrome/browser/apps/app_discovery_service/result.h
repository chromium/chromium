// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RESULT_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RESULT_H_

#include <memory>
#include <string>

namespace apps {

enum class AppSource;
class GameExtras;
class PlayExtras;

// Can be overridden by Sources that have unique fields.
class SourceExtras {
 public:
  virtual ~SourceExtras() = default;

  // When adding a new App Source with unique fields, override SourceExtras and
  // add a safe downcast here like so:
  // virtual FooExtras* AsFooExtras { return nullptr; }

  virtual std::unique_ptr<SourceExtras> Clone() = 0;

  // Safe downcasts:
  virtual GameExtras* AsGameExtras();
  virtual PlayExtras* AsPlayExtras();
};

class Result {
 public:
  Result(AppSource app_source,
         const std::string& icon_id,
         const std::u16string& app_title,
         std::unique_ptr<SourceExtras> source_extras);
  Result(const Result&);
  Result& operator=(const Result&);
  Result(Result&&);
  Result& operator=(Result&&);
  ~Result();

  // The endpoint from which the app was fetched from.
  AppSource GetAppSource() const;

  // The identifier used by the AppSource to identify an app icon.
  // For the Almanac fetcher this is an icon url.
  // For the legacy game fetcher this is a uuid.
  const std::string& GetIconId() const;

  // The title of the app to display to users.
  const std::u16string& GetAppTitle() const;

  // Extra fields that are unique to a single source can be found in that
  // source's individual implementation of SourceExtras.
  // Clients can use this function to get a provider's unique fields.
  // e.g. Play extras are defined in the class PlayExtras and are accessed
  // through: GetSourceExtras->AsPlayExtras();
  SourceExtras* GetSourceExtras() const;

 private:
  AppSource app_source_;
  std::string icon_id_;
  std::u16string app_title_;
  std::unique_ptr<SourceExtras> source_extras_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RESULT_H_

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_UPDATER_STATE_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_UPDATER_STATE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/update_client/update_client_errors.h"

namespace component_updater {

class UpdaterState {
 public:
  using Attributes = base::flat_map<std::string, std::string>;

  // Returns a map of items representing the state of an updater.
  // If |is_machine| is true, this indicates that the updater state corresponds
  // to the machine instance of the updater. Returns nullptr on
  // the platforms and builds where this feature is not supported.
  static Attributes GetState(bool is_machine);

  ~UpdaterState();

 private:
  FRIEND_TEST_ALL_PREFIXES(UpdaterStateTest, SerializeChromePerUser);
  FRIEND_TEST_ALL_PREFIXES(UpdaterStateTest, SerializeChromium);
  FRIEND_TEST_ALL_PREFIXES(UpdaterStateTest, UpdaterNamePerUser);

  struct State {
    State();
    State(const State&);
    State& operator=(const State&);
    ~State();

    std::string updater_name;
    base::Version updater_version;
    base::Time last_autoupdate_started;
    base::Time last_checked;
    bool is_autoupdate_check_enabled = false;
    int update_policy = 0;
    update_client::CategorizedError last_update_check_error = {};
  };

  class StateReader {
   public:
    static std::unique_ptr<StateReader> Create(bool is_machine);

    // Returns the state of the Chrome updater.
    State Read(bool is_machine) const;

    virtual ~StateReader() = default;

   private:
    virtual std::string GetUpdaterName() const = 0;
    virtual base::Version GetUpdaterVersion(bool is_machine) const = 0;
    virtual bool IsAutoupdateCheckEnabled() const = 0;
    virtual base::Time GetUpdaterLastStartedAU(bool is_machine) const = 0;
    virtual base::Time GetUpdaterLastChecked(bool is_machine) const = 0;
    virtual int GetUpdatePolicy() const = 0;
    virtual update_client::CategorizedError GetLastUpdateCheckError() const = 0;
  };

#if BUILDFLAG(IS_MAC)
  class StateReaderKeystone final : public StateReader {
   private:
    // Overrides for StateReader.
    std::string GetUpdaterName() const override;
    base::Version GetUpdaterVersion(bool is_machine) const override;
    bool IsAutoupdateCheckEnabled() const override;
    base::Time GetUpdaterLastStartedAU(bool is_machine) const override;
    base::Time GetUpdaterLastChecked(bool is_machine) const override;
    int GetUpdatePolicy() const override;
    update_client::CategorizedError GetLastUpdateCheckError() const override;
  };
#elif BUILDFLAG(IS_WIN)
  class StateReaderOmaha final : public StateReader {
   private:
    // Overrides for StateReader.
    std::string GetUpdaterName() const override;
    base::Version GetUpdaterVersion(bool is_machine) const override;
    bool IsAutoupdateCheckEnabled() const override;
    base::Time GetUpdaterLastStartedAU(bool is_machine) const override;
    base::Time GetUpdaterLastChecked(bool is_machine) const override;
    int GetUpdatePolicy() const override;
    update_client::CategorizedError GetLastUpdateCheckError() const override;
  };
#endif
  class StateReaderChromiumUpdater final : public StateReader {
   public:
    explicit StateReaderChromiumUpdater(base::Value::Dict parsed_json);

   private:
    // Overrides for StateReader.
    std::string GetUpdaterName() const override;
    base::Version GetUpdaterVersion(bool is_machine) const override;
    bool IsAutoupdateCheckEnabled() const override;
    base::Time GetUpdaterLastStartedAU(bool is_machine) const override;
    base::Time GetUpdaterLastChecked(bool is_machine) const override;
    int GetUpdatePolicy() const override;
    update_client::CategorizedError GetLastUpdateCheckError() const override;

    base::Time FindTimeKey(std::string_view key) const;
    const base::Value::Dict parsed_json_;
  };

  explicit UpdaterState(bool is_machine);

  // Builds the map of state attributes by serializing the state of this object.
  Attributes Serialize() const;

  static std::optional<State> ReadState(bool is_machine);

  static std::string GetUpdaterName();
  static base::Version GetUpdaterVersion(bool is_machine);
  static bool IsAutoupdateCheckEnabled();
  static base::Time GetUpdaterLastStartedAU(bool is_machine);
  static base::Time GetUpdaterLastChecked(bool is_machine);
  static int GetUpdatePolicy();

  static std::string NormalizeTimeDelta(base::TimeDelta delta);

  // True if the updater is installed per-machine.
  bool is_machine_ = false;

  std::optional<State> state_;
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_UPDATER_STATE_H_

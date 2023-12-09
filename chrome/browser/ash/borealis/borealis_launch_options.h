// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_LAUNCH_OPTIONS_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_LAUNCH_OPTIONS_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

class Profile;

namespace borealis {

// Switch for adding an extra disk to be used with borealis.
extern const char kExtraDiskSwitch[];

// The BorealisLaunchOptions processes a string representation of the borealis'
// runtime options into a struct, for use in the creation/lifetime of a
// BorealisContext.
//
// The string configuration is a comma-separated key=val list e.g.:
//
//                   op1=val1,opt2=val2,opt3=val3
//
// The valid options are recorded in the .cc file and go/borealis-care.
class BorealisLaunchOptions {
 public:
  // Struct containing the various ways Borealis can be modified at runtime.
  // These options are per-launch.
  struct Options {
    Options();
    Options(const Options&);
    ~Options();

    std::optional<base::FilePath> extra_disk = std::nullopt;

    bool auto_shutdown = true;
  };

  explicit BorealisLaunchOptions(Profile* profile);

  // Get the options for a borealis launch. This will almost always be the above
  // defaults in production, but it is necessary to allow changing these for
  // devs/testers.
  //
  // This process will not fail. Any malformed options or issues will result in
  // the default Options being returned.
  void Build(
      base::OnceCallback<void(BorealisLaunchOptions::Options)> callback) const;

  // Options are normally given on chrome's command-line. This method is used to
  // override the command-line. This is not strictly "for testing", but it is
  // used by the Autotest Privtate API.
  void ForceOptions(std::string opts_string);

 private:
  std::string options_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_LAUNCH_OPTIONS_H_

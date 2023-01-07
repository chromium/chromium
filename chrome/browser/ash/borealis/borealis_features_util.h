// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_FEATURES_UTIL_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_FEATURES_UTIL_H_

#include <memory>
#include <string>
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"

namespace borealis {

struct HardwareStatsA {};

// A helper class that makes verifying tokens/hardware easier. Users can extend
// this class and write a method like Check() that calls all the helpers.
class TokenHardwareChecker {
 public:
  struct Data {
    Data(std::string token_hash,
         std::string board,
         std::string model,
         std::string cpu,
         uint64_t memory);
    Data(const Data& other);
    ~Data();

    std::string token_hash;
    std::string board;
    std::string model;
    std::string cpu;
    uint64_t memory;
  };

  // A hashing function used for creating tokens.
  static std::string H(std::string input, const std::string& salt);

  // A method for constructing the Data object asynchronously.
  static void GetData(std::string token_hash,
                      base::OnceCallback<void(Data)> callback);

  explicit TokenHardwareChecker(Data token_hardware);

  // These members are intended to be accessed by a subclass that
  // implements a more complicated checking mechanism.

  bool TokenHashMatches(const std::string& salt,
                        const std::string& expected) const;
  bool IsBoard(const std::string& board) const;
  bool BoardIn(base::flat_set<std::string> boards) const;
  bool IsModel(const std::string& model) const;
  bool ModelIn(base::flat_set<std::string> models) const;
  bool CpuRegexMatches(const std::string& cpu_regex) const;
  bool HasMemory(uint64_t mem_bytes) const;

 private:
  const Data token_hardware_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_FEATURES_UTIL_H_

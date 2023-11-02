// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_INFRA_DESCRIBED_H_
#define CHROME_BROWSER_ASH_BOREALIS_INFRA_DESCRIBED_H_

#include <string>

// TODO(b/172501195): Make these available outside namespace borealis.
namespace borealis {

// A very common kind of error is a non-user-facing string (i.e. something you
// want to log) and an |E| typed error (like a status enum). |E| should be
// trivially copyable.
template <typename E>
class Described {
 public:
  // Creates an |error| described by the given |description|.
  Described(E error, std::string description)
      : error_(error), description_(std::move(description)) {}

  E error() const { return error_; }

  const std::string& description() const { return description_; }

  // Converts a Described error from |E| to |F|, by prepending |prefix| to
  // |this|'s description.
  template <typename F>
  Described<F> Into(F error, std::string prefix) {
    return Described<F>(error, prefix + ": " + description());
  }

 private:
  E error_;
  std::string description_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_INFRA_DESCRIBED_H_

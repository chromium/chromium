// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SCOPED_OBSERVATION_TRAITS_INTERNAL_H_
#define BASE_SCOPED_OBSERVATION_TRAITS_INTERNAL_H_

#include <type_traits>

namespace base::internal {

struct HasAddAndRemoveObserverMethodsHelper {
  template <class Source, class Observer>
  static auto Validate(Source* source, Observer* observer)
      -> decltype(source->AddObserver(observer),
                  source->RemoveObserver(observer),
                  std::true_type());

  template <class...>
  static auto Validate(...) -> std::false_type;
};

template <class Source, class Observer>
inline constexpr bool HasAddAndRemoveObserverMethods =
    decltype(HasAddAndRemoveObserverMethodsHelper::Validate<Source, Observer>(
        nullptr,
        nullptr))::value;

}  // namespace base::internal

#endif  // BASE_SCOPED_OBSERVATION_TRAITS_INTERNAL_H_

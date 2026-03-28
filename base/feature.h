// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FEATURE_H_
#define BASE_FEATURE_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/feature_buildflags.h"
#include "base/feature_internal.h"
#include "build/build_config.h"

#if BUILDFLAG(ENABLE_BANNED_BASE_FEATURE_PREFIX)
#include "base/logging.h"
#endif

namespace base {

class FeatureList;

// Recommended macros for declaring and defining features and parameters:
//
// - `kFeature` is the C++ identifier that will be used for the `base::Feature`.
// - `name` is the feature name, which must be globally unique. This name is
//   used to enable/disable features via experiments and command-line flags.
//   Names should use CamelCase-style naming, e.g. "MyGreatFeature".
// - `default_state` is the default state to use for the feature, i.e.
//   `base::FEATURE_DISABLED_BY_DEFAULT` or `base::FEATURE_ENABLED_BY_DEFAULT`.
//   As noted above, the actual runtime state may differ from the default state,
//   due to field trials or command-line switches.

// Provides a forward declaration for `kFeature` in a header file, e.g.
//
//   BASE_DECLARE_FEATURE(kMyFeature);
//
// If the feature needs to be marked as exported, i.e. it is referenced by
// multiple components, then write:
//
//   COMPONENT_EXPORT(MY_COMPONENT) BASE_DECLARE_FEATURE(kMyFeature);
#define BASE_DECLARE_FEATURE(kFeature) \
  extern constinit const base::Feature kFeature

// Provides a definition for `kFeature` with `name` and `default_state`, e.g.
//
// This macro can be used in two ways:
//
// 1. With two arguments, to define a feature whose name is derived from the C++
//    identifier. This form is preferred, as it avoids repeating the feature
//    name and helps prevent typos.
//
//      BASE_FEATURE(kMyFeature, base::FEATURE_DISABLED_BY_DEFAULT);
//
//    This is equivalent to:
//
//      BASE_FEATURE(kMyFeature, "MyFeature",
//                   base::FEATURE_DISABLED_BY_DEFAULT);
//
// 2. With three arguments, to explicitly specify the C++ identifier and the
//    name of the feature. This form should be used only if the feature needs
//    to have a C++ identifier that does not match the feature name, which
//    should be rare.
//
//      BASE_FEATURE(kMyFeature, "MyFeatureName",
//                   base::FEATURE_DISABLED_BY_DEFAULT);
//
// Features should *not* be defined in header files; do not use this macro in
// header files.
#define BASE_FEATURE(...)                        \
  BASE_FEATURE_INTERNAL_GET_FEATURE_MACRO(       \
      __VA_ARGS__, BASE_FEATURE_INTERNAL_3_ARGS, \
      BASE_FEATURE_INTERNAL_2_ARGS)(__VA_ARGS__)

// Provides a forward declaration for `feature_object_name` in a header file,
// e.g.
//
//   BASE_DECLARE_FEATURE_PARAM(int, kMyFeatureParam);
//
// If the feature needs to be marked as exported, i.e. it is referenced by
// multiple components, then write:
//
//   COMPONENT_EXPORT(MY_COMPONENT)
//   BASE_DECLARE_FEATURE_PARAM(int, kMyFeatureParam);
//
// This macro enables optimizations to make the second and later calls faster,
// but requires additional memory uses. If you obtain the parameter only once,
// you can instantiate base::FeatureParam directly, or can call
// base::GetFieldTrialParamByFeatureAsInt or equivalent functions for other
// types directly.
#define BASE_DECLARE_FEATURE_PARAM(T, feature_object_name) \
  extern constinit const base::FeatureParam<T> feature_object_name

// Provides a definition for `feature_object_name` with `T`, `feature`, `name`
// and `default_value`, with an internal parsed value cache.
//
// This macro can be used in two ways:
//
// 1. With four arguments, to define a feature param whose string name is
//    derived from its C++ identifier. This form is preferred, as it avoids
//    repeating the param name and helps prevent typos.
//
//      BASE_FEATURE_PARAM(int, kMyFeatureParam, &kMyFeature, 0);
//
//    This is equivalent to:
//
//      BASE_FEATURE_PARAM(int, kMyFeatureParam, &kMyFeature,
//                         "MyFeatureParam", 0);
//
// 2. With five arguments, to explicitly specify the string name of the
//    parameter. This form should be used only if the parameter needs to have
//    a string name that does not match the C++ identifier (should be rare).
//
//      BASE_FEATURE_PARAM(int, kMyFeatureParam, &kMyFeature,
//                         "my_feature_param", 0);
//
// `T` is a parameter type, one of bool, int, size_t, double, std::string, and
// base::TimeDelta. Enum types are not supported for now.
//
// It should *not* be defined in header files; do not use this macro in header
// files.
//
// WARNING: If the feature is not enabled, the parameter is not set, or set to
// an invalid value (per the param type), then Get() will return the default
// value passed to this C++ macro. In particular this will typically return the
// default value regardless of the server-side config in control groups.
#define BASE_FEATURE_PARAM(...)                        \
  BASE_FEATURE_INTERNAL_GET_FEATURE_PARAM_MACRO(       \
      __VA_ARGS__, BASE_FEATURE_PARAM_INTERNAL_5_ARGS, \
      BASE_FEATURE_PARAM_INTERNAL_4_ARGS)(__VA_ARGS__)

// Same as BASE_FEATURE_PARAM() but used for enum type parameters with on extra
// argument, `options`. See base::FeatureParam<Enum> template declaration in
// //base/metrics/field_trial_params.h for `options`' details.
#define BASE_FEATURE_ENUM_PARAM(...)                        \
  BASE_FEATURE_INTERNAL_GET_FEATURE_ENUM_PARAM_MACRO(       \
      __VA_ARGS__, BASE_FEATURE_ENUM_PARAM_INTERNAL_6_ARGS, \
      BASE_FEATURE_ENUM_PARAM_INTERNAL_5_ARGS)(__VA_ARGS__)

// Specifies whether a given feature is enabled or disabled by default.
// NOTE: The actual runtime state may be different, due to a field trial or a
// command line switch.
enum FeatureState {
  FEATURE_DISABLED_BY_DEFAULT,
  FEATURE_ENABLED_BY_DEFAULT,
};

// The Feature struct is used to define the default state for a feature. There
// must only ever be one struct instance for a given feature name—generally
// defined as a constant global variable or file static. Declare and define
// features using the `BASE_DECLARE_FEATURE()` and `BASE_FEATURE()` macros
// above, as there are some subtleties involved.
//
// Feature constants are internally mutable, as this allows them to contain a
// mutable member to cache their override state, while still remaining declared
// as const. This cache member allows for significantly faster IsEnabled()
// checks.
//
// However, the "Mutable Constants" check [1] detects this as a regression,
// because this usually means that a readonly symbol is put in writable memory
// when readonly memory would be more efficient.
//
// The performance gains of the cache are large enough to offset the downsides
// to having the symbols in bssdata rather than rodata. Use LOGICALLY_CONST to
// suppress the "Mutable Constants" check.
//
// [1]:
// https://crsrc.org/c/docs/speed/binary_size/android_binary_size_trybot.md#Mutable-Constants
struct BASE_EXPORT LOGICALLY_CONST Feature {
  // The type used to store the cached state of a feature. This is a uint32_t
  // that is packed with the override state, logging information, and a caching
  // context ID.
  // See the comments on `cached_value` below for more details.
  using FeatureStateCache = uint32_t;
  static constexpr FeatureStateCache kCachedLogGeneralMask = 0x00010000;
  static constexpr FeatureStateCache kCachedLogEarlyMask = 0x00020000;

  constexpr Feature(const char* name,
                    FeatureState default_state,
                    internal::FeatureMacroHandshake)
      : name(name), default_state(default_state) {
#if BUILDFLAG(ENABLE_BANNED_BASE_FEATURE_PREFIX)
    if (std::string_view(name).starts_with(
            BUILDFLAG(BANNED_BASE_FEATURE_PREFIX))) {
      LOG(FATAL) << "Invalid feature name " << name << " starts with "
                 << BUILDFLAG(BANNED_BASE_FEATURE_PREFIX);
    }
#endif  // BUILDFLAG(ENABLE_BANNED_BASE_FEATURE_PREFIX)
  }

  // Non-copyable since:
  // - there should be only one `Feature` instance per unique name.
  // - a `Feature` contains internal cached state about the override state.
  Feature(const Feature&) = delete;
  Feature& operator=(const Feature&) = delete;

  // The name of the feature. This should be unique to each feature and is used
  // for enabling/disabling features via command line flags and experiments.
  // It is strongly recommended to use CamelCase style for feature names, e.g.
  // "MyGreatFeature".
  const char* const name;

  // The default state (i.e. enabled or disabled) for this feature.
  // NOTE: The actual runtime state may be different, due to a field trial or a
  // command line switch.
  const FeatureState default_state;

 private:
  friend class FeatureList;

  // A packed value where the first 8 bits represent the `OverrideState` of this
  // feature, the next 8 bits are reserved for flags (e.g. logging), and the
  // last 16 bits are a caching context ID used to allow ScopedFeatureLists to
  // invalidate these cached values in testing. A value of 0 in the caching
  // context ID field indicates that this value has never been looked up and
  // cached, a value of 1 indicates this value contains the cached
  // `OverrideState` that was looked up via `base::FeatureList`, and any other
  // value indicate that this cached value is only valid for a particular
  // ScopedFeatureList instance.
  //
  // Packing these values into a uint32_t makes it so that atomic operations
  // performed on this fields can be lock free.
  //
  // The override state stored in this field is only used if the current
  // `FeatureList::caching_context_` field is equal to the lower 16 bits of the
  // packed cached value. Otherwise, the override state is looked up in the
  // feature list and the cache is updated.
  // The logging bits (16 and 17) are used to ensure that we only log the
  // feature access once per session, even if the cached value is invalidated.
  mutable std::atomic<FeatureStateCache> cached_value = 0;
};

}  // namespace base

#endif  // BASE_FEATURE_H_

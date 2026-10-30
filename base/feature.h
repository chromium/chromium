// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FEATURE_H_
#define BASE_FEATURE_H_

#include <atomic>
#include <cstdint>
#include <string_view>

#include "base/base_export.h"
#include "base/containers/span.h"
#include "base/feature_buildflags.h"
#include "base/feature_internal.h"
#include "base/logging.h"
#include "build/build_config.h"

namespace base {

class FeatureList;
class FeatureListTest;

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
// Note that the `extern "C"` here is necessary to make Features accessible
// across FFI boundaries (motivated, in particular, by the need for Rust
// interop.)
//
// If the feature needs to be marked as exported, i.e. it is referenced by
// multiple components, then write:
//
//   COMPONENT_EXPORT(MY_COMPONENT) BASE_DECLARE_FEATURE(kMyFeature);
#define BASE_DECLARE_FEATURE(kFeature) \
  extern "C" constinit const base::Feature kFeature

// Provides a definition for `kFeature` with `name` and `default_state`, e.g.
//
// There are two versions of this macro: one for runtime static features, whose
// state is fixed at initialization time, and one for runtime mutable features,
// whose state may be updated at runtime as new Variations seeds are received.
//
// TODO(crbug.com/482450632): Runtime mutable features are currently only
// supported in the browser process (see crbug.com/536851532 for child-process
// support).
//
// The feature state of runtime mutable features, and any related parameters,
// may only be queried from the main sequence. Additionally, runtime mutable
// features must have their runtime mutability explicitly enabled via the
// `FeatureList::EnableRuntimeMutability()` function call (see
// `PlatformFieldTrials::EnableRuntimeMutableFeatures()` and its
// platform-specific overrides).
//
// This macro can be used in two ways:
//
// 1. With two arguments, to define a feature whose name is derived from the C++
//    identifier. This form is preferred, as it avoids repeating the feature
//    name and helps prevent typos.
//
//      BASE_FEATURE(kMyFeature, base::FEATURE_DISABLED_BY_DEFAULT);
//      BASE_RUNTIME_MUTABLE_FEATURE(kMyRuntimeMutableFeature,
//                                   base::FEATURE_DISABLED_BY_DEFAULT);
//
//    This is equivalent to:
//
//      BASE_FEATURE(kMyFeature, "MyFeature",
//                   base::FEATURE_DISABLED_BY_DEFAULT);
//      BASE_RUNTIME_MUTABLE_FEATURE(kMyRuntimeMutableFeature,
//                                   "MyRuntimeMutableFeature",
//                                   base::FEATURE_DISABLED_BY_DEFAULT);
//
// 2. With three arguments, to explicitly specify the C++ identifier and the
//    name of the feature. This form should be used only if the feature needs
//    to have a C++ identifier that does not match the feature name, which
//    should be rare.
//
//      BASE_FEATURE(kMyFeature, "MyFeatureName",
//                   base::FEATURE_DISABLED_BY_DEFAULT);
//      BASE_RUNTIME_MUTABLE_FEATURE(kMyRuntimeMutableFeature,
//                                   "MyRuntimeMutableFeatureName",
//                                   base::FEATURE_DISABLED_BY_DEFAULT);
//
// Features should *not* be defined in header files; do not use this macro in
// header files.
#define BASE_FEATURE(...)                        \
  BASE_FEATURE_INTERNAL_GET_FEATURE_MACRO(       \
      __VA_ARGS__, BASE_FEATURE_INTERNAL_3_ARGS, \
      BASE_FEATURE_INTERNAL_2_ARGS)(false, __VA_ARGS__)
#define BASE_RUNTIME_MUTABLE_FEATURE(...)        \
  BASE_FEATURE_INTERNAL_GET_FEATURE_MACRO(       \
      __VA_ARGS__, BASE_FEATURE_INTERNAL_3_ARGS, \
      BASE_FEATURE_INTERNAL_2_ARGS)(true, __VA_ARGS__)

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
// and `default_value`, with an internal parsed value cache (bypassed if
// `feature` is defined with `BASE_RUNTIME_MUTABLE_FEATURE`).
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
// base::TimeDelta. Enum types are not supported for now. Note that size_t is
// backed by int in the underlying API, and thus should not exceed INT_MAX.
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
  // The feature is disabled only for the countries specified. Defaults to
  // enabled if Chrome does not have information about the current country.
  // Use only together with `BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS`.
  FEATURE_DISABLED_FOR_COUNTRIES,
  // The feature is enabled only for the countries specified. Defaults to
  // disabled if Chrome does not have information about the current country.
  // Use only together with `BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS`.
  FEATURE_ENABLED_FOR_COUNTRIES,
  kFeatureStateMaxValue = FEATURE_ENABLED_FOR_COUNTRIES,
};

static_assert(FeatureState::kFeatureStateMaxValue < 4,
              "FeatureState values must fit in the 2-bit default-state field "
              "of the packed feature state");

constexpr bool IsCountrySpecificFeatureState(FeatureState state) {
  switch (state) {
    case FEATURE_DISABLED_BY_DEFAULT:
    case FEATURE_ENABLED_BY_DEFAULT:
      return false;
    case FEATURE_DISABLED_FOR_COUNTRIES:
    case FEATURE_ENABLED_FOR_COUNTRIES:
      return true;
  }
}

// The Feature struct is used to define the default state for a feature. There
// must only ever be one struct instance for a given feature name—generally
// defined as a constant global variable or file static. Declare and define
// features using the `BASE_DECLARE_FEATURE()` and `BASE_FEATURE()` macros
// above, as there are some subtleties involved.
//
// A Feature is immutable, so it can be placed in read-only memory. The state
// that `FeatureList` writes, such as the cached resolved state, lives in a
// separate atomic word that `state` points to.
struct BASE_EXPORT Feature {
  // The type used to store the cached state of a feature. This is a uint32_t
  // that is packed with the resolved enabled state, explicit-override marker,
  // logging and mutability flags, default state, and a caching context ID.
  // See the comments on `state` below for more details.
  using FeatureStateCache = uint32_t;

  constexpr Feature(const char* name,
                    std::atomic<FeatureStateCache>* state,
                    internal::FeatureMacroHandshake handshake)
      : name(name), state(state) {
#if BUILDFLAG(ENABLE_BANNED_BASE_FEATURE_PREFIX)
    if (std::string_view(name).starts_with(
            BUILDFLAG(BANNED_BASE_FEATURE_PREFIX))) {
      LOG(FATAL) << "Invalid feature name " << name << " starts with "
                 << BUILDFLAG(BANNED_BASE_FEATURE_PREFIX);
    }
#endif  // BUILDFLAG(ENABLE_BANNED_BASE_FEATURE_PREFIX)
  }

  // Non-copyable since:
  // - there should be only one `Feature` instance per unique name; its address
  //   is its identity (see `FeatureList::CheckFeatureIdentity()`).
  // - a `Feature` holds the only pointer to its state storage.
  Feature(const Feature&) = delete;
  Feature& operator=(const Feature&) = delete;

  // Returns true if this feature is a runtime mutable feature.
  bool IsRuntimeMutable() const;

  // Returns true if this feature was accessed before the FeatureList was
  // initialized.
  bool WasAccessedEarly() const;

  // The name of the feature. This should be unique to each feature and is used
  // for enabling/disabling features via command line flags and experiments.
  // It is strongly recommended to use CamelCase style for feature names, e.g.
  // "MyGreatFeature".
  const char* const name;

  // Returns the default state (i.e. enabled or disabled) for this feature.
  // NOTE: The actual runtime state may be different, due to a field trial or a
  // command line switch.
  FeatureState GetDefaultState() const;

 private:
  friend class FeatureList;
  friend class FeatureListTest;

  // Points to a packed value where bits 24 and 25 hold the resolved enabled
  // state and whether the feature was explicitly overridden. Bits 16 through 23
  // hold flags (logging, runtime mutability, and the default state), and bits 0
  // through 15 hold a caching context ID used to allow ScopedFeatureLists to
  // invalidate these cached values in testing. A value of 0 in the caching
  // context ID field indicates that this value has never been looked up and
  // cached, a value of 1 indicates this value contains a state cached via
  // `base::FeatureList`, and any other value indicates that this cached value
  // is only valid for a particular ScopedFeatureList instance.
  //
  // Packing these values into a uint32_t makes it so that atomic operations
  // performed on this fields can be lock free.
  //
  // The cached state is only used if the current
  // `FeatureList::caching_context_` field is equal to the lower 16 bits of the
  // packed cached value. Otherwise, the state is looked up in the feature list
  // and the cache is updated. The logging bits (16 and 17) are used to ensure
  // that we only log the feature access once per session, even if the cached
  // value is invalidated.
  //
  // RAW_PTR_EXCLUSION: The feature macros guarantee that the state storage has
  // static storage duration.
  RAW_PTR_EXCLUSION std::atomic<FeatureStateCache>* const state;
};

// `FeatureWithCountryRestriction` allows enabling or disabling a feature for a
// given set of countries. This country set is referenced in the `countries`
// member of the struct.
//
// Querying the feature state (via `FeatureList::IsEnabled`) or using it with
// `FeatureParam` works exactly the same as for `Feature` (due to implicit
// upcasting). Having a separate class is only a memory/binary-size
// optimization. Differences are:
// - Use BASE_DECLARE_FEATURE_WITH_COUNTRY_RESTRICTIONS and
//   BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS to declare and define
//   `FeatureWithCountryRestriction`.
// - Use only `FEATURE_DISABLED_FOR_COUNTRIES` and
//   `FEATURE_ENABLED_FOR_COUNTRIES` as default states. Everything else should
//   use the BASE_FEATURE macros.
// - Pass a non-empty list of two-letter, lower-case country codes as string
//   literals to define the country set.
// - Currently, passing an explicit name string (similar to the 3-argument
//   version of BASE_FEATURE) and runtime mutability are not supported.
//
// Example usage:
// - Declaration in a header:
//
//   BASE_DECLARE_FEATURE_WITH_COUNTRY_RESTRICTIONS(kMyFeature);
//
// - Definition:
//
//   BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(kMyFeature,
//       base::FEATURE_ENABLED_FOR_COUNTRIES, "us", "de", "fr");
//
//   The resulting feature is default-enabled if and only if the permanent
//   variations country is either US, Germany, or France.
struct BASE_EXPORT FeatureWithCountryRestriction : public Feature {
  constexpr FeatureWithCountryRestriction(
      const char* feature_name,
      std::atomic<FeatureStateCache>* state,
      base::span<const std::string_view> countries_list,
      internal::FeatureMacroHandshake handshake)
      : Feature(feature_name, state, handshake), countries(countries_list) {}

  FeatureWithCountryRestriction(const FeatureWithCountryRestriction&) = delete;
  FeatureWithCountryRestriction& operator=(
      const FeatureWithCountryRestriction&) = delete;

  // RAW_PTR_EXCLUSION: base::FeatureWithCountryRestriction is constructed via
  // macros that guarantee static storage duration for the span. The exclusion
  // is also necessary to avoid errors due to non-trivial exit-time destructors.
  RAW_PTR_EXCLUSION const base::span<const std::string_view> countries;
};

}  // namespace base

#endif  // BASE_FEATURE_H_

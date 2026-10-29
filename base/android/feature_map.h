// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_FEATURE_MAP_H_
#define BASE_ANDROID_FEATURE_MAP_H_

#include <string>

#include "base/base_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"

namespace base::android {

// A FeatureMap is a mapping from base:Feature's names to a pointer to the
// base:Feature.
// This is necessary because in Java, features flags are identified by the
// feature name, a string, so calls from Java to (for example) check the state
// of a feature flag need to convert the string to a non-owning Feature*.
// Similarly, a FeatureMap can map "{feature_name}:{param_name}" strings to
// base::FeatureParam pointers, so that Java can retrieve param values through
// FeatureParam::Get().
// Each component should have its own FeatureMap.
class BASE_EXPORT FeatureMap {
 public:
  explicit FeatureMap(base::span<const Feature* const> features_exposed_to_java,
                      base::span<const FeatureParam<bool>* const>
                          boolean_params_exposed_to_java = {},
                      base::span<const FeatureParam<int>* const>
                          int_params_exposed_to_java = {},
                      base::span<const FeatureParam<double>* const>
                          double_params_exposed_to_java = {},
                      base::span<const FeatureParam<std::string>* const>
                          string_params_exposed_to_java = {});
  ~FeatureMap();

  // Map a |feature_name| to a Feature*.
  const Feature* FindFeatureExposedToJava(const std::string& feature_name);

  // Map a |feature_name| and a |param_name| to a FeatureParam<T>*.
  const FeatureParam<bool>* FindBooleanParamExposedToJava(
      const std::string& feature_name,
      const std::string& param_name);
  const FeatureParam<int>* FindIntParamExposedToJava(
      const std::string& feature_name,
      const std::string& param_name);
  const FeatureParam<double>* FindDoubleParamExposedToJava(
      const std::string& feature_name,
      const std::string& param_name);
  const FeatureParam<std::string>* FindStringParamExposedToJava(
      const std::string& feature_name,
      const std::string& param_name);

 private:
  flat_map<std::string_view, raw_ptr<const Feature, CtnExperimental>> mapping_;

  // Keyed by "{feature_name}:{param_name}". Unlike `mapping_`, the keys are
  // built at runtime, so they must be owned by the maps.
  flat_map<std::string, raw_ptr<const FeatureParam<bool>, CtnExperimental>>
      boolean_params_mapping_;
  flat_map<std::string, raw_ptr<const FeatureParam<int>, CtnExperimental>>
      int_params_mapping_;
  flat_map<std::string, raw_ptr<const FeatureParam<double>, CtnExperimental>>
      double_params_mapping_;
  flat_map<std::string,
           raw_ptr<const FeatureParam<std::string>, CtnExperimental>>
      string_params_mapping_;
};

}  // namespace base::android

#endif  // BASE_ANDROID_FEATURE_MAP_H_

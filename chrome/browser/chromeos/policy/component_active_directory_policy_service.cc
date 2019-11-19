// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/component_active_directory_policy_service.h"

#include <iterator>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/dbus/login_manager/policy_descriptor.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/registry_dict.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

using RetrieveResult = ComponentActiveDirectoryPolicyRetriever::RetrieveResult;
using RetrieveResponseType =
    ComponentActiveDirectoryPolicyRetriever::ResponseType;

namespace {

constexpr char kKeyMandatory[] = "Policy";
constexpr char kKeyRecommended[] = "Recommended";

// Policy level and corresponding path.
struct Level {
  PolicyLevel level;
  const char* json_key;
};

constexpr Level kLevels[] = {
    {POLICY_LEVEL_MANDATORY, kKeyMandatory},
    {POLICY_LEVEL_RECOMMENDED, kKeyRecommended},
};

// Returns the level out of |kLevels| with matching |json_key|. Uses
// case-insensitive comparison because |json_key| usually comes from Windows
// registry, which also uses case-insensitive comparison. Returns nullptr if
// no matching level is found.
const Level* FindMatchingLevel(const std::string& json_key) {
  for (const Level& level : kLevels) {
    if (base::EqualsCaseInsensitiveASCII(json_key, level.json_key))
      return &level;
  }
  return nullptr;
}

// Gets the policy_value() from the em::PolicyData nested in a serialized
// em::PolicyFetchResponse blob. Returns an empty string on error.
std::string GetPolicyValue(const std::string& policy_fetch_response_blob) {
  em::PolicyFetchResponse policy_fetch_response;
  em::PolicyData policy_data;
  if (!policy_fetch_response.ParseFromString(policy_fetch_response_blob) ||
      policy_fetch_response.policy_data().empty() ||
      !policy_data.ParseFromString(policy_fetch_response.policy_data()) ||
      policy_data.policy_value().empty()) {
    LOG(ERROR) << "Could not parse fetch response";
    return std::string();
  }
  return policy_data.policy_value();
}

// Parses |json| to a base::DictionaryValue. Returns nullptr and prints errors
// on failure.
std::unique_ptr<base::DictionaryValue> ParseJsonToDict(
    const std::string& json) {
  std::string json_reader_error_message;
  std::unique_ptr<base::Value> value =
      base::JSONReader::ReadAndReturnErrorDeprecated(
          json, base::JSON_ALLOW_TRAILING_COMMAS, nullptr /* error_code_out */,
          &json_reader_error_message);
  if (!value) {
    LOG(ERROR) << "Could not parse policy value as JSON: "
               << json_reader_error_message;
    return nullptr;
  }

  // Convert to a dictionary.
  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(std::move(value));
  if (!dict) {
    LOG(ERROR) << "The JSON policy value is not a dictionary.";
    return nullptr;
  }

  return dict;
}

// Gets the policy value from the |policy_fetch_response_blob|, parses it as
// JSON, uses |schema| to convert some values (e.g. 0/1 registry ints to bools)
// and puts it into |policy| for the given |scope|.
bool ParsePolicy(const std::string& policy_fetch_response_blob,
                 const Schema& schema,
                 PolicyScope scope,
                 PolicyMap* policy) {
  // Get the policy value from the fetch response.
  std::string policy_value = GetPolicyValue(policy_fetch_response_blob);
  if (policy_value.empty())
    return false;

  // Parse the policy value, which should be a JSON string. The JSON is expected
  // to be formatted like
  //   { "Policy":{ "Name1":Value1 }, "Recommended":{ "Name2":Value2 } }
  // and matches the format on Chrome for Windows (see Load3rdPartyPolicy in
  // PolicyLoaderWin). On a side note, this is different from the schema sent
  // down from DMServer for Google cloud managed devices, which is
  //   { "Name1": { "Value":Value1 },
  //     "Name2": { "Value":Value2, "Level":"Recommended" } }
  // (see ParsePolicy in ComponentCloudPolicyStore).
  std::unique_ptr<base::DictionaryValue> dict = ParseJsonToDict(policy_value);

  // Search for "Policy" and "Recommended" keys in dict, perform some type
  // conversions on the sub-dicts and put them into |policy|.
  for (const auto& it : dict->DictItems()) {
    const Level* level = FindMatchingLevel(it.first);
    if (!level) {
      LOG(WARNING) << "Unknown key '" << it.first
                   << "'. Expected 'Policy' or 'Recommended'.";
      continue;
    }

    // Type-convert policy values using schema information. Since GPO and
    // registry don't support certain types, they use other types that have to
    // be converted to the types specified in the schema:
    //   string -> double for 'number'  type policies
    //   int    -> bool   for 'boolean' type policies
    std::unique_ptr<base::Value> converted_value =
        ConvertRegistryValue(it.second, schema);
    std::unique_ptr<base::DictionaryValue> converted_dict =
        base::DictionaryValue::From(std::move(converted_value));
    if (!converted_dict) {
      LOG(ERROR) << "Failed to filter JSON policy at level " << level->json_key;
      continue;
    }

    // Put the policy into the right spot.
    policy->LoadFrom(converted_dict.get(), level->level, scope,
                     POLICY_SOURCE_ACTIVE_DIRECTORY);
  }
  return true;
}

}  // namespace

ComponentActiveDirectoryPolicyService::Delegate::~Delegate() {}

ComponentActiveDirectoryPolicyService::ComponentActiveDirectoryPolicyService(
    PolicyScope scope,
    PolicyDomain domain,
    login_manager::PolicyAccountType account_type,
    const std::string& account_id,
    Delegate* delegate,
    SchemaRegistry* schema_registry)
    : scope_(scope),
      domain_(domain),
      account_type_(account_type),
      account_id_(account_id),
      delegate_(delegate),
      schema_registry_(schema_registry) {
  // Observe the schema registry for keeping |current_schema_map_| up to date.
  schema_registry_->AddObserver(this);
  UpdateFromSchemaRegistry();
}

ComponentActiveDirectoryPolicyService::
    ~ComponentActiveDirectoryPolicyService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  schema_registry_->RemoveObserver(this);
}

void ComponentActiveDirectoryPolicyService::RetrievePolicies() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ignore if we're not ready yet. Note that UpdateFromSchemaRegistry() calls
  // RetrievePolicies() as soon as the schemas are ready.
  if (!current_schema_map_)
    return;

  // If there's an in-flight retrieval request, don't cancel it, but call
  // RetrievePolicies() again once it finishes. Otherwise, requests might never
  // finish if new requests are scheduled in quick succession.
  if (policy_retriever_) {
    should_retrieve_again_ = true;
    return;
  }

  // Get all namespaces from the schema map that match our |domain_|.
  std::vector<PolicyNamespace> namespaces;
  for (const auto& domain_kv : current_schema_map_->GetDomains()) {
    if (domain_ == domain_kv.first) {
      const ComponentMap& component_map = domain_kv.second;
      for (const auto& component_kv : component_map)
        namespaces.emplace_back(domain_, component_kv.first /* component_id */);
    }
  }

  // Retrieve the corresponding policies from Session Manager.
  DVLOG(1) << "Retrieving policies for " << namespaces.size() << " namespaces";
  policy_retriever_ = std::make_unique<ComponentActiveDirectoryPolicyRetriever>(
      account_type_, account_id_, std::move(namespaces),
      base::BindOnce(&ComponentActiveDirectoryPolicyService::OnPolicyRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
  policy_retriever_->Start();
}

void ComponentActiveDirectoryPolicyService::OnSchemaRegistryReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateFromSchemaRegistry();
}

void ComponentActiveDirectoryPolicyService::OnSchemaRegistryUpdated(
    bool has_new_schemas) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateFromSchemaRegistry();
}

void ComponentActiveDirectoryPolicyService::UpdateFromSchemaRegistry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignore if registry is not ready yet.
  if (!schema_registry_->IsReady())
    return;

  DVLOG(1) << "Updating schema map";
  current_schema_map_ = schema_registry_->schema_map();

  RetrievePolicies();
}

void ComponentActiveDirectoryPolicyService::OnPolicyRetrieved(
    std::vector<RetrieveResult> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Convert the list of JSON policy strings to a PolicyBundle.
  auto policy = std::make_unique<PolicyBundle>();
  for (const RetrieveResult& result : results) {
    if (result.response != RetrieveResponseType::SUCCESS)
      continue;

    // Always create an entry for the ns, even if policy is empty or invalid.
    PolicyMap& policy_map = policy->Get(result.ns);

    // Ignore empty policy (SessionManager returns empty policy if the policy
    // requested does not exist, e.g. extension with no policy set).
    if (result.policy_fetch_response_blob.empty())
      continue;

    // Get schema. It's possible that we've requested policy for a namespace
    // that's not in the schema registry anymore, e.g. if the schema map
    // changed while the request was in flight. Just silently ignore.
    const Schema* schema = current_schema_map_->GetSchema(result.ns);
    if (!schema) {
      DVLOG(1) << "No schema for component id " << result.ns.component_id;
      continue;
    }

    // Parse JSON policy, do some type conversions and store policy.
    if (!ParsePolicy(result.policy_fetch_response_blob, *schema, scope_,
                     &policy_map)) {
      LOG(ERROR) << "Failed to parse policy for component id "
                 << result.ns.component_id;
      policy_map.Clear();
    }
  }

  // Filter policy by the corresponding schema and send it to the delegate.
  FilterAndInstallPolicy(std::move(policy));

  // Reset retrieval request. If RetrievePolicies() was called while the current
  // retrieval requests was in-flight, queue it again here.
  policy_retriever_.reset();
  if (should_retrieve_again_) {
    should_retrieve_again_ = false;
    DCHECK(!policy_retriever_);
    RetrievePolicies();
  }
}

void ComponentActiveDirectoryPolicyService::FilterAndInstallPolicy(
    std::unique_ptr<PolicyBundle> policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  policy_ = std::move(policy);

  // Remove policies that don't match the schema.
  current_schema_map_->FilterBundle(policy_.get());

  DVLOG(1) << "Installed policy (count = "
           << std::distance(policy_->begin(), policy_->end()) << ")";
  delegate_->OnComponentActiveDirectoryPolicyUpdated();
}

}  // namespace policy

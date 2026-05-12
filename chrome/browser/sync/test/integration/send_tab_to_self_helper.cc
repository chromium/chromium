// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/send_tab_to_self_helper.h"

#include <map>
#include <numeric>
#include <sstream>

#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/hit_test_region_observer.h"

namespace send_tab_to_self_helper {

namespace {

bool WaitForElementFocus(content::WebContents* web_contents,
                         const std::string& id) {
  // Wait until the element is actually focused. This ensures that subsequent
  // typing events are targeted correctly.
  base::test::ScopedRunLoopTimeout focus_timeout(FROM_HERE, base::Seconds(10));
  return base::test::RunUntil([&]() {
    return content::EvalJs(
               web_contents,
               base::StringPrintf("document.activeElement.id === '%s'",
                                  id.c_str()))
        .ExtractBool();
  });
}

bool WaitForElementValue(content::WebContents* web_contents,
                         const std::string& id,
                         const std::string& expected_value) {
  // Verify that the value was set correctly. Keystrokes aren't synchronized
  // with JS and they take time to process, so we wait until the value matches.
  // A 10s timeout is used to ensure the test doesn't hang indefinitely on
  // failure.
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Seconds(10));
  return base::test::RunUntil([&]() {
    content::EvalJsResult actual_value =
        GetFormFieldValueById(web_contents, id);
    return actual_value.is_ok() &&
           actual_value.ExtractString() == expected_value;
  });
}

}  // namespace

SendTabToSelfUrlChecker::SendTabToSelfUrlChecker(
    send_tab_to_self::SendTabToSelfSyncService* service,
    const GURL& url)
    : url_(url), service_(service) {
  DCHECK(service);
  observation_.Observe(service->GetSendTabToSelfModel());
}

SendTabToSelfUrlChecker::~SendTabToSelfUrlChecker() = default;

bool SendTabToSelfUrlChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for data for url '" + url_.spec() + "' to be populated.";

  send_tab_to_self::SendTabToSelfModel* model =
      service_->GetSendTabToSelfModel();
  for (const std::string& guid : model->GetAllGuids()) {
    if (model->GetEntryByGUID(guid)->GetURL() == url_) {
      return true;
    }
  }
  return false;
}

void SendTabToSelfUrlChecker::SendTabToSelfModelLoaded() {
  CheckExitCondition();
}

void SendTabToSelfUrlChecker::EntriesAddedRemotely(
    const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
        new_entries) {
  CheckExitCondition();
}

void SendTabToSelfUrlChecker::EntriesRemovedRemotely(
    const std::vector<std::string>& guids_removed) {
  CheckExitCondition();
}

SendTabToSelfUrlOpenedChecker::SendTabToSelfUrlOpenedChecker(
    send_tab_to_self::SendTabToSelfSyncService* service,
    const GURL& url)
    : url_(url), service_(service) {
  DCHECK(service);
  observation_.Observe(service->GetSendTabToSelfModel());
}

SendTabToSelfUrlOpenedChecker::~SendTabToSelfUrlOpenedChecker() = default;

bool SendTabToSelfUrlOpenedChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for data for url '" + url_.spec() + "' to be marked opened.";

  send_tab_to_self::SendTabToSelfModel* model =
      service_->GetSendTabToSelfModel();
  for (const std::string& guid : model->GetAllGuids()) {
    const send_tab_to_self::SendTabToSelfEntry* entry =
        model->GetEntryByGUID(guid);
    if (entry->GetURL() == url_ && entry->IsOpened()) {
      return true;
    }
  }
  return false;
}

void SendTabToSelfUrlOpenedChecker::SendTabToSelfModelLoaded() {
  CheckExitCondition();
}

void SendTabToSelfUrlOpenedChecker::EntriesAddedRemotely(
    const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
        new_entries) {
  CheckExitCondition();
}

void SendTabToSelfUrlOpenedChecker::EntriesRemovedRemotely(
    const std::vector<std::string>& guids_removed) {
  CheckExitCondition();
}

void SendTabToSelfUrlOpenedChecker::EntriesOpenedRemotely(
    const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
        opened_entries) {
  CheckExitCondition();
}

SendTabToSelfModelEqualityChecker::SendTabToSelfModelEqualityChecker(
    send_tab_to_self::SendTabToSelfSyncService* service0,
    send_tab_to_self::SendTabToSelfSyncService* service1)
    : service0_(service0), service1_(service1) {
  DCHECK(service0);
  DCHECK(service1);
  observation0_.Observe(service0->GetSendTabToSelfModel());
  observation1_.Observe(service1->GetSendTabToSelfModel());
}

SendTabToSelfModelEqualityChecker::~SendTabToSelfModelEqualityChecker() =
    default;

bool SendTabToSelfModelEqualityChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for services to converge";

  const send_tab_to_self::SendTabToSelfModel* model0 =
      service0_->GetSendTabToSelfModel();
  const send_tab_to_self::SendTabToSelfModel* model1 =
      service1_->GetSendTabToSelfModel();

  if (model0->GetAllGuids() != model1->GetAllGuids()) {
    return false;
  }
  for (const std::string& guid : model0->GetAllGuids()) {
    const send_tab_to_self::SendTabToSelfEntry* entry0 =
        model0->GetEntryByGUID(guid);
    const send_tab_to_self::SendTabToSelfEntry* entry1 =
        model1->GetEntryByGUID(guid);

    DCHECK_NE(entry0, nullptr);
    DCHECK_NE(entry1, nullptr);

    if (entry0->GetGUID() != entry1->GetGUID() ||
        entry0->GetURL() != entry1->GetURL() ||
        entry0->GetTitle() != entry1->GetTitle() ||
        entry0->GetSharedTime() != entry1->GetSharedTime() ||
        entry0->GetDeviceName() != entry1->GetDeviceName()) {
      return false;
    }
  }
  return true;
}

void SendTabToSelfModelEqualityChecker::SendTabToSelfModelLoaded() {
  CheckExitCondition();
}

void SendTabToSelfModelEqualityChecker::EntriesAddedRemotely(
    const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
        new_entries) {
  CheckExitCondition();
}

void SendTabToSelfModelEqualityChecker::EntriesRemovedRemotely(
    const std::vector<std::string>& guids_removed) {
  CheckExitCondition();
}

SendTabToSelfActiveChecker::SendTabToSelfActiveChecker(
    send_tab_to_self::SendTabToSelfSyncService* service)
    : service_(service) {
  DCHECK(service);
  observation_.Observe(service->GetSendTabToSelfModel());
}

SendTabToSelfActiveChecker::~SendTabToSelfActiveChecker() = default;

bool SendTabToSelfActiveChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for model to be active.";
  return service_->GetSendTabToSelfModel()->IsReady();
}

void SendTabToSelfActiveChecker::SendTabToSelfModelLoaded() {
  CheckExitCondition();
}

void SendTabToSelfActiveChecker::EntriesAddedRemotely(
    const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
        new_entries) {
  CheckExitCondition();
}

void SendTabToSelfActiveChecker::EntriesRemovedRemotely(
    const std::vector<std::string>& guids_removed) {
  CheckExitCondition();
}

SendTabToSelfMultiDeviceActiveChecker::SendTabToSelfMultiDeviceActiveChecker(
    syncer::DeviceInfoTracker* tracker)
    : tracker_(tracker) {
  tracker_->AddObserver(this);
}

SendTabToSelfMultiDeviceActiveChecker::
    ~SendTabToSelfMultiDeviceActiveChecker() {
  tracker_->RemoveObserver(this);
}

bool SendTabToSelfMultiDeviceActiveChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for multiple devices to be active.";
  const absl::flat_hash_map<syncer::DeviceInfo::FormFactor, int>
      device_count_by_type = tracker_->CountActiveDevicesByType();
  const int total = std::accumulate(
      device_count_by_type.begin(), device_count_by_type.end(), 0,
      [](int sum, const auto& pair) { return sum + pair.second; });
  return total > 1;
}

void SendTabToSelfMultiDeviceActiveChecker::OnDeviceInfoChange() {
  CheckExitCondition();
}

SendTabToSelfDeviceDisabledChecker::SendTabToSelfDeviceDisabledChecker(
    syncer::DeviceInfoTracker* tracker,
    const std::string& device_guid)
    : tracker_(tracker), device_guid_(device_guid) {
  tracker_->AddObserver(this);
}

SendTabToSelfDeviceDisabledChecker::~SendTabToSelfDeviceDisabledChecker() {
  tracker_->RemoveObserver(this);
}

bool SendTabToSelfDeviceDisabledChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for device to have send_tab_to_self disabled";
  const syncer::DeviceInfo* device_info = tracker_->GetDeviceInfo(device_guid_);
  return device_info && !device_info->send_tab_to_self_receiving_enabled();
}

void SendTabToSelfDeviceDisabledChecker::OnDeviceInfoChange() {
  CheckExitCondition();
}

SendTabToSelfUrlDeletedChecker::SendTabToSelfUrlDeletedChecker(
    send_tab_to_self::SendTabToSelfSyncService* service,
    const GURL& url)
    : url_(url), service_(service) {
  DCHECK(service);
  observation_.Observe(service->GetSendTabToSelfModel());
}

SendTabToSelfUrlDeletedChecker::~SendTabToSelfUrlDeletedChecker() = default;

bool SendTabToSelfUrlDeletedChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for data for url '" + url_.spec() + "' to be deleted.";

  send_tab_to_self::SendTabToSelfModel* model =
      service_->GetSendTabToSelfModel();
  DCHECK(model);
  // Checks each URL in the model and returns passes if URL in question is not
  // found.
  for (const std::string& guid : model->GetAllGuids()) {
    if (model->GetEntryByGUID(guid)->GetURL() == url_) {
      return false;
    }
  }
  return true;
}

void SendTabToSelfUrlDeletedChecker::SendTabToSelfModelLoaded() {
  // This ensures that the URL being inspected is present when the model loads.
  std::ostringstream s;
  DCHECK(!IsExitConditionSatisfied(&s));
}

void SendTabToSelfUrlDeletedChecker::EntriesAddedRemotely(
    const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
        new_entries) {}

void SendTabToSelfUrlDeletedChecker::EntriesRemovedRemotely(
    const std::vector<std::string>& guids_removed) {
  CheckExitCondition();
}

SendTabToSelfScrollChecker::SendTabToSelfScrollChecker(
    content::WebContents* web_contents,
    const std::string& element_id)
    : web_contents_(web_contents), element_id_(element_id) {
  timer_.Start(
      FROM_HERE, base::Milliseconds(100),
      base::BindRepeating(&SendTabToSelfScrollChecker::CheckExitCondition,
                          base::Unretained(this)));
}

SendTabToSelfScrollChecker::~SendTabToSelfScrollChecker() = default;

bool SendTabToSelfScrollChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for element '" << element_id_ << "' to be in viewport.";
  // We check if any part of the element is vertically within the viewport.
  content::EvalJsResult result =
      content::EvalJs(web_contents_, content::JsReplace(R"(
    (() => {
      const target = document.getElementById($1);
      if (!target) return false;
      const rect = target.getBoundingClientRect();
      return rect.top < window.innerHeight && rect.bottom >= 0;
    })()
  )",
                                                        element_id_));

  return result.is_ok() && result.ExtractBool();
}

AutofillFieldsSeenChecker::AutofillFieldsSeenChecker(
    content::WebContents* web_contents,
    std::map<std::string, std::string> expected_fields)
    : web_contents_(web_contents),
      expected_fields_(std::move(expected_fields)) {
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(web_contents);
  observation_.Observe(client,
                       autofill::ScopedAutofillManagersObservation::
                           InitializationPolicy::kObservePreexistingManagers);
}

AutofillFieldsSeenChecker::~AutofillFieldsSeenChecker() = default;

bool AutofillFieldsSeenChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for autofill fields to be seen and values matched: ";
  for (const auto& [id, value] : expected_fields_) {
    *os << id << "='" << value << "' ";
  }

  std::map<std::string, std::string> remaining_fields = expected_fields_;

  web_contents_->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
    autofill::ContentAutofillDriver* driver =
        autofill::ContentAutofillDriver::GetForRenderFrameHost(rfh);
    if (!driver) {
      return;
    }
    driver->GetAutofillManager().ForEachCachedForm(
        [&](const autofill::FormStructure& form) {
          for (const std::unique_ptr<autofill::AutofillField>& field :
               form.fields()) {
            const std::string id = base::UTF16ToUTF8(field->id_attribute());
            if (const std::string* expected_value =
                    base::FindOrNull(remaining_fields, id)) {
              if (*expected_value == base::UTF16ToUTF8(field->value())) {
                remaining_fields.erase(id);
              }
            }
          }
        });
  });

  if (!remaining_fields.empty()) {
    *os << "(Missing or value mismatch: ";
    for (const auto& [id, value] : remaining_fields) {
      *os << id << " ";
    }
    *os << ")";
  }

  return remaining_fields.empty();
}

void AutofillFieldsSeenChecker::OnAfterFormsSeen(
    autofill::AutofillManager& manager,
    base::span<const autofill::FormGlobalId> updated_forms,
    base::span<const autofill::FormGlobalId> removed_forms) {
  CheckExitCondition();
}

void AutofillFieldsSeenChecker::OnAfterTextFieldValueChanged(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form,
    autofill::FieldGlobalId field) {
  CheckExitCondition();
}

content::EvalJsResult GetFormFieldValueById(content::WebContents* web_contents,
                                            const std::string& id) {
  return content::EvalJs(
      web_contents,
      base::StringPrintf("document.getElementById('%s').value;", id.c_str()));
}

testing::AssertionResult PopulateFormField(content::WebContents* web_contents,
                                           const std::string& id,
                                           const std::string& value) {
  // Input event to a page may not work right after a page load, so
  // SimulateEndOfPaintHoldingOnPrimaryMainFrame() is used as workaround.
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents);

  // Hit test data is required for `SimulateMouseClickOrTapElementWithId` to
  // correctly determine the element's coordinates within the renderer. Without
  // this, the click might be sent to (0, 0) or fail to target the element,
  // leading to focus issues and subsequent typing failures.
  {
    base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Seconds(10));
    content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
  }

  // Click the element to focus it. This is more robust than focus() in some
  // environments.
  content::SimulateMouseClickOrTapElementWithId(web_contents, id);

  if (!WaitForElementFocus(web_contents, id)) {
    return testing::AssertionFailure()
           << "Timed out waiting for focus on field with id " << id
           << ". Active element id: "
           << content::EvalJs(web_contents, "document.activeElement.id")
                  .ExtractString();
  }

  for (char c : value) {
    content::SimulateCharTyped(web_contents, c);
  }

  if (!WaitForElementValue(web_contents, id, value)) {
    content::EvalJsResult actual_value =
        GetFormFieldValueById(web_contents, id);
    return testing::AssertionFailure()
           << "Field with id " << id << " has value "
           << (actual_value.is_ok() ? actual_value.ExtractString() : "ERROR")
           << " but expected " << value;
  }

  return testing::AssertionSuccess();
}

}  // namespace send_tab_to_self_helper

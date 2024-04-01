// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_utils.h"

#include <string>

#include "chrome/browser/download/bubble/download_bubble_accessible_alerts_map.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_ui_model.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/mock_download_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;
using Alert = DownloadBubbleAccessibleAlertsMap::Alert;
using State = download::DownloadItem::DownloadState;

std::unique_ptr<NiceMock<download::MockDownloadItem>> InitDownloadItem(
    download::DownloadItem::DownloadState state,
    download::DownloadDangerType danger_type =
        download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
  auto item = std::make_unique<NiceMock<download::MockDownloadItem>>();
  EXPECT_CALL(*item, GetGuid())
      .WillRepeatedly(ReturnRefOfCopy(std::string("1")));
  EXPECT_CALL(*item, GetFileNameToReportUser())
      .WillRepeatedly(
          Return(base::FilePath(FILE_PATH_LITERAL("download.pdf"))));
  EXPECT_CALL(*item, GetState()).WillRepeatedly(Return(state));
  EXPECT_CALL(*item, IsDangerous())
      .WillRepeatedly(
          Return(danger_type != download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  EXPECT_CALL(*item, GetDangerType()).WillRepeatedly(Return(danger_type));
  EXPECT_CALL(*item, PercentComplete()).WillRepeatedly(Return(50));

  return item;
}

MATCHER_P2(MatchesAlert, alert_substring, urgency, "") {
  return arg.text.find(alert_substring) != std::u16string::npos &&
         arg.urgency == urgency;
}

MATCHER(AlertIsEmpty, "") {
  return arg.text.empty();
}

TEST(DownloadBubbleUtilsTest, GetAccessibleAlertForModel_InProgressNormal) {
  auto item = InitDownloadItem(State::IN_PROGRESS);
  DownloadItemModel model{
      item.get(), std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()};

  Alert alert = GetAccessibleAlertForModel(model);
  EXPECT_THAT(alert,
              MatchesAlert(u"50%", Alert::Urgency::kAlertWhenAppropriate));
}

TEST(DownloadBubbleUtilsTest, GetAccessibleAlertForModel_InProgressPaused) {
  auto item = InitDownloadItem(State::IN_PROGRESS);
  EXPECT_CALL(*item, IsPaused()).WillRepeatedly(Return(true));
  DownloadItemModel model{
      item.get(), std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()};

  Alert alert = GetAccessibleAlertForModel(model);
  EXPECT_THAT(alert, MatchesAlert(u"paused", Alert::Urgency::kAlertSoon));
}

TEST(DownloadBubbleUtilsTest, GetAccessibleAlertForModel_Interrupted) {
  auto item = InitDownloadItem(State::INTERRUPTED);
  EXPECT_CALL(*item, GetLastReason())
      .WillRepeatedly(Return(download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED));
  DownloadItemModel model{
      item.get(), std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()};

  Alert alert = GetAccessibleAlertForModel(model);
  EXPECT_THAT(alert, MatchesAlert(u"unsuccessful", Alert::Urgency::kAlertSoon));
}

TEST(DownloadBubbleUtilsTest, GetAccessibleAlertForModel_Complete) {
  auto item = InitDownloadItem(State::COMPLETE);
  DownloadItemModel model{
      item.get(), std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()};

  Alert alert = GetAccessibleAlertForModel(model);
  EXPECT_THAT(alert, MatchesAlert(u"complete", Alert::Urgency::kAlertSoon));
}

TEST(DownloadBubbleUtilsTest, GetAccessibleAlertForModel_Cancelled) {
  auto item = InitDownloadItem(State::CANCELLED);
  DownloadItemModel model{
      item.get(), std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()};

  Alert alert = GetAccessibleAlertForModel(model);
  EXPECT_THAT(alert, MatchesAlert(u"cancelled", Alert::Urgency::kAlertSoon));
}

TEST(DownloadBubbleUtilsTest, GetAccessibleAlertForModel_Dangerous) {
  auto item = InitDownloadItem(
      State::IN_PROGRESS, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT);
  DownloadItemModel model{
      item.get(), std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()};

  Alert alert = GetAccessibleAlertForModel(model);
  EXPECT_THAT(alert, MatchesAlert(u"dangerous", Alert::Urgency::kAlertSoon));
}

TEST(DownloadBubbleUtilsTest, GetAccessibleAlertForModel_PromptForScanning) {
  auto item = InitDownloadItem(
      State::IN_PROGRESS, download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING);
  DownloadItemModel model{
      item.get(), std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()};

  Alert alert = GetAccessibleAlertForModel(model);
  EXPECT_THAT(alert, MatchesAlert(u"scanning", Alert::Urgency::kAlertSoon));
}

TEST(DownloadBubbleUtilsTest, GetAccessibleAlertForModel_DeepScanning) {
  auto item = InitDownloadItem(State::IN_PROGRESS,
                               download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING);
  EXPECT_CALL(*item, IsDangerous()).WillRepeatedly(Return(false));
  DownloadItemModel model{
      item.get(), std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()};

  Alert alert = GetAccessibleAlertForModel(model);
  EXPECT_THAT(alert,
              MatchesAlert(u"being scanned", Alert::Urgency::kAlertSoon));
}

TEST(DownloadBubbleUtilsTest, GetAccessibleAlertForModel_Insecure) {
  auto item = InitDownloadItem(State::IN_PROGRESS);
  EXPECT_CALL(*item, IsInsecure()).WillRepeatedly(Return(true));
  DownloadItemModel model{
      item.get(), std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()};

  Alert alert = GetAccessibleAlertForModel(model);
  EXPECT_THAT(alert, MatchesAlert(u"can't be downloaded securely",
                                  Alert::Urgency::kAlertSoon));
}

}  // namespace

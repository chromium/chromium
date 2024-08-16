// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/hats/hats_dialog.h"

#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_finch_helper.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/version/version_loader.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace ash {

namespace {

// Default width/height ratio of screen size.
const int kDefaultWidth = 384;
const int kDefaultHeight = 428;

// The state specific UMA enumerations
const int kSurveyDisplayedEnumeration = 2;
const int kSurveyCompleteEnumeration = 3;

// Possible requested actions from the HTML+JS client.
// Client is ready to close the page.
const char kClientActionLoad[] = "load";
// Client is ready to close the page.
const char kClientActionClose[] = "close";
// Client is ready to close the page after completing the survey.
const char kClientActionComplete[] = "complete";
// There was an unhandled error and we need to log and close the page.
const char kClientActionUnhandledError[] = "survey-loading-error";
// A smiley was selected, so we'd like to track that.
const char kClientQuestionAnswered[] = "answer-";
const char kClientQuestionAnsweredRegex[] = "answer-(\\d+)-((?:\\d+,?)+)";
const char kClientQuestionAnsweredScoreRegex[] = "(\\d+),?";

constexpr char kCrOSHaTSURL[] =
    "https://storage.googleapis.com/chromeos-hats-web-stable/index.html";

}  // namespace

// Only log a histogram value if there is a histogram name provided.
void LogHistogram(const std::string& histogram_name, int enumeration) {
  if (!histogram_name.empty()) {
    base::UmaHistogramSparse(histogram_name, enumeration);
  }
}

// static
bool HatsDialog::ParseAnswer(const std::string& input,
                             int* question,
                             std::vector<int>* scores) {
  std::string question_num_string;
  std::string_view all_scores_string;
  if (!RE2::FullMatch(input, kClientQuestionAnsweredRegex, &question_num_string,
                      &all_scores_string))
    return false;

  if (!base::StringToInt(question_num_string, question) || *question <= 0 ||
      *question > 10) {
    LOG(ERROR) << "Can't parse Survey score";
    return false;
  }

  std::string score_string;
  while (RE2::FindAndConsume(
      &all_scores_string, kClientQuestionAnsweredScoreRegex, &score_string)) {
    int score;
    if (!base::StringToInt(score_string, &score) || score <= 0 || score > 100) {
      LOG(ERROR) << "Can't parse Survey score";
      return false;
    }
    scores->push_back(score);
  }

  return true;
}

bool HatsDialog::HandleClientTriggeredAction(
    const std::string& action,
    const std::string& histogram_name) {
  DVLOG(1) << "HandleClientTriggeredAction: Received " << action;

  // Page asks to be closed.
  if (action == kClientActionClose) {
    return true;
  }

  // An unhandled error in our client, log and close.
  if (base::StartsWith(action, kClientActionUnhandledError)) {
    LOG(ERROR) << "Error while loading a HaTS Survey " << action;
    return true;
  }

  // Page successfully loaded the survey.
  if (action == kClientActionLoad) {
    LogHistogram(histogram_name, kSurveyDisplayedEnumeration);
    return false;
  }

  // Page asks to be closed after completing the survey.
  if (action == kClientActionComplete) {
    LogHistogram(histogram_name, kSurveyCompleteEnumeration);
    return true;
  }

  // A question was answered
  if (base::StartsWith(action, kClientQuestionAnswered)) {
    int question;
    std::vector<int> question_scores;
    if (!ParseAnswer(action, &question, &question_scores)) {
      return false;  // It's a client error, but don't close the page.
    }

    for (int score : question_scores) {
      // The enumeration is specified as `QQNN`, where `QQ` is the question
      // number and `NN` is the answer index. Therefore, we can calculate this
      // value via `QQ * 100 + NN`.
      // Note: The `ParseAnswer` function guarantees that the score will be
      // in the range [1, 100].
      int enumeration = (question * 100) + score;
      LogHistogram(histogram_name, enumeration);
    }

    return false;  // Don't close the page.
  }

  // Future proof - ignore unimplemented commands.
  return false;
}

HatsDialog::HatsDialog(const std::string& trigger_id,
                       const std::string& histogram_name,
                       const std::string& site_context)
    : trigger_id_(trigger_id), histogram_name_(histogram_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  set_allow_default_context_menu(false);
  set_can_close(true);
  set_can_resize(false);
  set_dialog_content_url(GURL(std::string(kCrOSHaTSURL) + "?emitAnswers=true&" +
                              site_context + "&trigger=" + trigger_id_));
  set_dialog_frame_kind(ui::WebDialogDelegate::FrameKind::kDialog);
  set_dialog_modal_type(ui::mojom::ModalType::kSystem);
  set_dialog_size(gfx::Size(kDefaultWidth, kDefaultHeight));
  set_show_close_button(true);
  set_show_dialog_title(false);
}

HatsDialog::~HatsDialog() = default;

void HatsDialog::Show(const std::string& trigger_id,
                      const std::string& histogram_name,
                      const std::string& site_context) {
  // HatsDialog is self-deleting via OnDialogClosed().
  chrome::ShowWebDialog(
      nullptr, ProfileManager::GetActiveUserProfile(),
      new HatsDialog(trigger_id, histogram_name, site_context));
}

void HatsDialog::OnLoadingStateChanged(WebContents* source) {
  // Only trigger actions when the URL changes
  if (action_ != source->GetURL().ref()) {
    action_ = source->GetURL().ref();
    if (HandleClientTriggeredAction(action_, histogram_name_)) {
      source->ClosePage();
    }
  }
}

}  // namespace ash

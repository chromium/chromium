// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/cxx17_backports.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "components/variations/synthetic_trials.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_list.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "sql/database.h"
#include "third_party/blink/public/common/features.h"

namespace {

constexpr base::FilePath::CharType kLiteVideoOptOutDBFilename[] =
    FILE_PATH_LITERAL("lite_video_opt_out.db");

const base::FilePath::CharType kHostDataUseDBName[] =
    FILE_PATH_LITERAL("data_reduction_proxy_leveldb");

void DeleteHostDataUseDatabaseOnDBThread(const base::FilePath& database_file) {
  base::DeleteFile(database_file);
}

// Deletes Previews opt-out database file. Opt-out database is no longer needed
// since Previews has been turned down.
void DeletePreviewsOptOutDatabaseOnDBThread(
    const base::FilePath& previews_optout_database_file) {
  sql::Database::Delete(previews_optout_database_file);
}

// Deletes LiteVideos opt-out database file. Opt-out database is no longer
// needed since LiteVideos has been turned down.
void DeleteLiteVideosOptOutDatabaseOnDBThread(
    const base::FilePath& optout_database_file) {
  sql::Database::Delete(optout_database_file);
}

}  // namespace

DataReductionProxyChromeSettings::DataReductionProxyChromeSettings(
    bool is_off_the_record_profile)
    : data_reduction_proxy::DataReductionProxySettings(
          is_off_the_record_profile) {
  DCHECK(!is_off_the_record_profile);
}

DataReductionProxyChromeSettings::~DataReductionProxyChromeSettings() {}

void DataReductionProxyChromeSettings::Shutdown() {}

void DataReductionProxyChromeSettings::InitDataReductionProxySettings(
    Profile* profile,
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Delete Previews OptOut database file.
  // Deletion started in M-91, and should run for few milestones.
  // TODO(https://crbug.com/1183505): Delete this logic.
  const base::FilePath& profile_path = profile->GetPath();
  db_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(DeletePreviewsOptOutDatabaseOnDBThread,
                     profile_path.Append(chrome::kPreviewsOptOutDBFilename)));

  db_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(DeleteLiteVideosOptOutDatabaseOnDBThread,
                     profile_path.Append(kLiteVideoOptOutDBFilename)));

  db_task_runner->PostTask(
      FROM_HERE, base::BindOnce(DeleteHostDataUseDatabaseOnDBThread,
                                profile_path.Append(kHostDataUseDBName)));

  data_reduction_proxy::DataReductionProxySettings::
      InitDataReductionProxySettings();
}

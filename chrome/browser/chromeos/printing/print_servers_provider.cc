// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_servers_provider.h"

#include <memory>
#include <set>
#include <vector>

#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/print_server.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

constexpr int kMaxRecords = 16;

struct TaskResults {
  int task_id;
  std::vector<PrintServer> servers;
};

// Parses |data|, a JSON blob, into a vector of PrintServers.  If |data| cannot
// be parsed, returns data with empty list of servers.
// This needs to run on a sequence that may block as it can be very slow.
TaskResults ParseData(int task_id, std::unique_ptr<std::string> data) {
  TaskResults task_data;
  task_data.task_id = task_id;

  if (!data) {
    LOG(WARNING) << "Received null data";
    return task_data;
  }

  // This could be really slow.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::JSONReader::ValueWithError value_with_error =
      base::JSONReader::ReadAndReturnValueWithError(
          *data, base::JSONParserOptions::JSON_PARSE_RFC);
  if (value_with_error.error_code != base::JSONReader::JSON_NO_ERROR) {
    LOG(WARNING) << "Failed to parse print servers policy ("
                 << value_with_error.error_message << ") on line "
                 << value_with_error.error_line << " at position "
                 << value_with_error.error_column;
    return task_data;
  }

  base::Value& json_blob = value_with_error.value.value();
  if (!json_blob.is_list()) {
    LOG(WARNING) << "Failed to parse print servers policy "
                 << "(an array was expected)";
    return task_data;
  }

  std::set<std::string> print_server_ids;
  std::set<GURL> print_server_urls;
  task_data.servers.reserve(json_blob.GetList().size());
  for (const base::Value& val : json_blob.GetList()) {
    if (!val.is_dict()) {
      LOG(WARNING) << "Entry in print servers policy skipped. "
                   << "Not a dictionary.";
      continue;
    }
    const std::string* id = val.FindStringKey("id");
    const std::string* url = val.FindStringKey("url");
    const std::string* name = val.FindStringKey("display_name");
    if (id == nullptr || url == nullptr || name == nullptr) {
      LOG(WARNING) << "Entry in print servers policy skipped. The following "
                   << "fields are required: id, url, display_name.";
      continue;
    }
    GURL gurl(*url);
    if (!gurl.is_valid()) {
      LOG(WARNING) << "Entry in print servers policy skipped. "
                   << "The following URL is invalid: " << *url;
      continue;
    }
    if (!gurl.SchemeIsHTTPOrHTTPS() && !gurl.SchemeIs("ipp") &&
        !gurl.SchemeIs("ipps")) {
      LOG(WARNING) << "Entry in print servers policy skipped. "
                   << "URL has unsupported scheme. Only the following "
                   << "schemes are supported: http, https, ipp, ipps";
      continue;
    }
    // Replaces ipp/ipps by http/https. IPP standard describes protocol built
    // on top of HTTP, so both types of addresses have the same meaning in the
    // context of IPP interface. Moreover, the URL must have http/https scheme
    // to pass IsStandard() test from GURL library (see "Validation of the URL
    // address" below).
    bool replaced_ipp_schema = false;
    if (gurl.SchemeIs("ipp")) {
      gurl = GURL("http" + url->substr(url->find_first_of(':')));
      replaced_ipp_schema = true;
    } else if (gurl.SchemeIs("ipps")) {
      gurl = GURL("https" + url->substr(url->find_first_of(':')));
    }
    // Validation of the URL address.
    if (!gurl.is_valid()) {
      LOG(WARNING) << "Entry in print servers policy skipped. "
                   << "The following URL is invalid: " << *url;
      continue;
    }
    // The default port for ipp is 631. If the schema ipp is replaced by http
    // and the port is not explicitly defined in the url, we have to overwrite
    // the default http port with the default ipp port. For ipps we do nothing
    // because implementers use the same port for ipps and https.
    if (replaced_ipp_schema && gurl.IntPort() == url::PORT_UNSPECIFIED) {
      GURL::Replacements replacement;
      replacement.SetPortStr("631");
      gurl = gurl.ReplaceComponents(replacement);
    }
    // Checks if server's ID and URL is not already used. If yes, a warning is
    // emitted and the record is skipped.
    if (print_server_ids.count(*id) || print_server_urls.count(gurl)) {
      LOG(WARNING) << "Entry in print servers policy skipped. There is "
                   << "already a record with the same ID (" << *id << ") or "
                   << "the same URL (" << gurl.spec() << ")";
      continue;
    }
    // Update the set of IDs and the set of URLs and add a new print server.
    print_server_ids.insert(*id);
    print_server_urls.insert(gurl);
    task_data.servers.emplace_back(*id, gurl, *name);
  }

  return task_data;
}

class PrintServersProviderImpl : public PrintServersProvider {
 public:
  PrintServersProviderImpl()
      : task_runner_(base::CreateSequencedTaskRunner(
            {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
             base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  ~PrintServersProviderImpl() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  void SetProfile(Profile* profile) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (profile_ != nullptr) {
      // Some unit tests may create more than one profile with the same user.
      return;
    }
    profile_ = profile;
    pref_change_registrar_.Init(profile->GetPrefs());
    // Bind UpdateWhitelist() method and call it once.
    pref_change_registrar_.Add(
        prefs::kExternalPrintServersWhitelist,
        base::BindRepeating(&PrintServersProviderImpl::UpdateWhitelist,
                            base::Unretained(this)));
    UpdateWhitelist();
  }

  void AddObserver(PrintServersProvider::Observer* observer) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    observers_.AddObserver(observer);
    observer->OnServersChanged(IsCompleted(), result_servers_);
  }

  void RemoveObserver(PrintServersProvider::Observer* observer) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    observers_.RemoveObserver(observer);
  }

  void ClearData() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    const bool previously_completed = IsCompleted();
    const bool previously_empty = result_servers_.empty();
    last_processed_task_ = ++last_received_task_;
    servers_.clear();
    result_servers_.clear();
    if (!(previously_completed && previously_empty)) {
      // Notify observers.
      for (auto& observer : observers_)
        observer.OnServersChanged(true, result_servers_);
    }
  }

  void SetData(std::unique_ptr<std::string> data) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    const bool previously_completed = IsCompleted();
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&ParseData, ++last_received_task_, std::move(data)),
        base::BindOnce(&PrintServersProviderImpl::OnComputationComplete,
                       weak_ptr_factory_.GetWeakPtr()));
    if (previously_completed) {
      // Notify observers.
      for (auto& observer : observers_)
        observer.OnServersChanged(false, result_servers_);
    }
  }

 private:
  // Returns true <=> there is no tasks being processed and there was at least
  // one call to SetData(...) or ClearData(...).
  bool IsCompleted() const {
    // The case when there is no calls to SetData(...) or ClearData(...).
    if (last_received_task_ == 0)
      return false;
    // The case when there is at least one unfinished task.
    if (last_processed_task_ != last_received_task_)
      return false;
    // The case when a profile is not set.
    if (profile_ == nullptr)
      return false;
    return true;
  }

  // Called when a new whitelist is available.
  void UpdateWhitelist() {
    whitelist_.clear();
    whitelist_is_set_ = false;
    // Fetch and parse the whitelist.
    const PrefService::Preference* pref = profile_->GetPrefs()->FindPreference(
        prefs::kExternalPrintServersWhitelist);
    if (pref != nullptr && !pref->IsDefaultValue()) {
      const base::ListValue* list =
          profile_->GetPrefs()->GetList(prefs::kExternalPrintServersWhitelist);
      if (list != nullptr) {
        whitelist_is_set_ = true;
        for (const base::Value& value : *list) {
          if (value.is_string()) {
            whitelist_.insert(value.GetString());
          }
        }
      }
    }
    // Calculate resultant list and notify observers in case of changes.
    const bool has_changes = CalculateResultantList();
    if (has_changes) {
      const bool is_completed = IsCompleted();
      for (auto& observer : observers_)
        observer.OnServersChanged(is_completed, result_servers_);
    }
  }

  // Recalculate the value of |result_servers_| field. Returns true if the new
  // list is different than the previous one.
  bool CalculateResultantList() {
    std::vector<PrintServer> new_servers;
    if (profile_ == nullptr) {
      // |result_servers_| remains empty when profile is not set.
      return false;
    }
    if (!whitelist_is_set_) {
      new_servers = servers_;
    } else {
      for (auto& print_server : servers_) {
        if (whitelist_.count(print_server.GetId())) {
          if (new_servers.size() == kMaxRecords) {
            LOG(WARNING) << "The list of resultant print servers read from "
                         << "policies is too long. Only the first "
                         << kMaxRecords << " print servers will be taken into "
                         << "account";
            break;
          }
          new_servers.push_back(print_server);
        }
      }
    }
    if (new_servers == result_servers_) {
      return false;
    }
    result_servers_ = std::move(new_servers);
    return true;
  }

  // Called on computation completion. |task_data| corresponds to finalized
  // task.
  void OnComputationComplete(TaskResults&& task_data) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (task_data.task_id <= last_processed_task_) {
      // The task is outdated (e.g.: ClearData() was called in the meantime).
      return;
    }
    last_processed_task_ = task_data.task_id;
    // IsCompleted() was false before (this task was pending).
    const bool is_complete = IsCompleted();
    if (!is_complete && servers_ == task_data.servers) {
      // No changes in the object's state.
      return;
    }
    servers_ = std::move(task_data.servers);
    const bool has_changes = CalculateResultantList();
    // Notify observers if something changed.
    if (is_complete || has_changes) {
      for (auto& observer : observers_)
        observer.OnServersChanged(is_complete, result_servers_);
    }
  }

  // The sequence used for parsing JSON and computing the list of servers.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Id of the last scheduled task.
  int last_received_task_ = 0;
  // Id of the last completed task.
  int last_processed_task_ = 0;
  // The current input list of servers.
  std::vector<PrintServer> servers_;
  // The current whitelist.
  bool whitelist_is_set_ = false;
  std::set<std::string> whitelist_;
  // The current resultant list of servers.
  std::vector<PrintServer> result_servers_;

  Profile* profile_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<PrintServersProvider::Observer>::Unchecked observers_;
  base::WeakPtrFactory<PrintServersProviderImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrintServersProviderImpl);
};

}  // namespace

// static
void PrintServersProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kExternalPrintServersWhitelist);
}

// static
std::unique_ptr<PrintServersProvider> PrintServersProvider::Create() {
  return std::make_unique<PrintServersProviderImpl>();
}

}  // namespace chromeos

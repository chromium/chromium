// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/braille_display_private/braille_controller_brlapi.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/braille_display_private/brlapi_connection.h"
#include "chrome/browser/extensions/api/braille_display_private/brlapi_keycode_map.h"
#include "chrome/browser/extensions/api/braille_display_private/stub_braille_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"

namespace extensions {
using content::BrowserThread;
namespace api {
namespace braille_display_private {

namespace {

// Delay between detecting a directory update and trying to connect
// to the brlapi.
constexpr base::TimeDelta kConnectionDelay = base::Milliseconds(500);

// How long to periodically retry connecting after a brltty restart.
// Some displays are slow to connect.
constexpr base::TimeDelta kConnectRetryTimeout = base::Seconds(20);

}  // namespace

// static
BrailleController* BrailleController::GetInstance() {
  BrailleControllerImpl* instance = BrailleControllerImpl::GetInstance();
  if (!instance->use_self_in_tests()) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(::switches::kTestType)) {
      return api::braille_display_private::StubBrailleController::GetInstance();
    }
  }
  return instance;
}

// static
BrailleControllerImpl* BrailleControllerImpl::GetInstance() {
  return base::Singleton<
      BrailleControllerImpl,
      base::LeakySingletonTraits<BrailleControllerImpl>>::get();
}

BrailleControllerImpl::BrailleControllerImpl() {
  create_brlapi_connection_function_ = base::BindOnce(
      &BrailleControllerImpl::CreateBrlapiConnection, base::Unretained(this));
}

BrailleControllerImpl::~BrailleControllerImpl() = default;

void BrailleControllerImpl::TryLoadLibBrlApi() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (skip_libbrlapi_so_load_ || libbrlapi_loader_.loaded())
    return;

  // This api version needs to match the one contained in
  // third_party/libbrlapi/brlapi.h.
  static const char* const kSupportedVersion = "libbrlapi.so.0.8";

  if (!libbrlapi_loader_.Load(kSupportedVersion)) {
    PLOG(WARNING) << "Couldn't load libbrlapi(" << kSupportedVersion << ")";
  }
}

std::unique_ptr<DisplayState> BrailleControllerImpl::GetDisplayState() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  StartConnecting();
  std::unique_ptr<DisplayState> display_state(new DisplayState);
  if (connection_.get() && connection_->Connected()) {
    unsigned int columns = 0;
    unsigned int rows = 0;
    if (!connection_->GetDisplaySize(&columns, &rows)) {
      Disconnect();
    } else if (rows * columns > 0) {
      // rows * columns == 0 means no display present.
      display_state->available = true;
      display_state->text_column_count = columns;
      display_state->text_row_count = rows;

      unsigned int cell_size = 0;
      connection_->GetCellSize(&cell_size);
      display_state->cell_size = cell_size;
    }
  }
  return display_state;
}

void BrailleControllerImpl::WriteDots(const std::vector<uint8_t>& cells,
                                      unsigned int cells_cols,
                                      unsigned int cells_rows) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (connection_ && connection_->Connected()) {
    // Row count and column count of current display.
    unsigned int columns = 0;
    unsigned int rows = 0;
    if (!connection_->GetDisplaySize(&columns, &rows)) {
      Disconnect();
    }
    std::vector<unsigned char> sized_cells(rows * columns, 0);
    unsigned int row_limit = std::min(rows, cells_rows);
    unsigned int col_limit = std::min(columns, cells_cols);
    for (unsigned int row = 0; row < row_limit; row++) {
      for (unsigned int col = 0;
           col < col_limit && (row * columns + col) < cells.size(); col++) {
        sized_cells[row * columns + col] = cells[row * columns + col];
      }
    }

    if (!connection_->WriteDots(sized_cells))
      Disconnect();
  }
}

void BrailleControllerImpl::AddObserver(BrailleObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&BrailleControllerImpl::StartConnecting,
                                    base::Unretained(this)))) {
    NOTREACHED_IN_MIGRATION();
  }
  observers_.AddObserver(observer);
}

void BrailleControllerImpl::RemoveObserver(BrailleObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

void BrailleControllerImpl::SetCreateBrlapiConnectionForTesting(
    CreateBrlapiConnectionFunction function) {
  if (function.is_null()) {
    create_brlapi_connection_function_ = base::BindOnce(
        &BrailleControllerImpl::CreateBrlapiConnection, base::Unretained(this));
  } else {
    create_brlapi_connection_function_ = std::move(function);
  }
}

void BrailleControllerImpl::PokeSocketDirForTesting() {
  OnSocketDirChangedOnIOThread();
}

void BrailleControllerImpl::StartConnecting() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (started_connecting_)
    return;
  started_connecting_ = true;
  TryLoadLibBrlApi();
  if (!libbrlapi_loader_.loaded() && !skip_libbrlapi_so_load_) {
    return;
  }

  if (!sequenced_task_runner_) {
    sequenced_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  }

  // Only try to connect after we've started to watch the
  // socket directory.  This is necessary to avoid a race condition
  // and because we don't retry to connect after errors that will
  // persist until there's a change to the socket directory (i.e.
  // ENOENT).
  sequenced_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&BrailleControllerImpl::StartWatchingSocketDirOnTaskThread,
                     base::Unretained(this)),
      base::BindOnce(&BrailleControllerImpl::TryToConnect,
                     base::Unretained(this)));
  ResetRetryConnectHorizon();
}

void BrailleControllerImpl::StartWatchingSocketDirOnTaskThread() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::FilePath brlapi_dir(BRLAPI_SOCKETPATH);
  if (!file_path_watcher_.Watch(
          brlapi_dir, base::FilePathWatcher::Type::kNonRecursive,
          base::BindRepeating(
              &BrailleControllerImpl::OnSocketDirChangedOnTaskThread,
              base::Unretained(this)))) {
    LOG(WARNING) << "Couldn't watch brlapi directory " << BRLAPI_SOCKETPATH;
  }
}

void BrailleControllerImpl::OnSocketDirChangedOnTaskThread(
    const base::FilePath& path,
    bool error) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (error) {
    LOG(ERROR) << "Error watching brlapi directory: " << path.value();
    return;
  }
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BrailleControllerImpl::OnSocketDirChangedOnIOThread,
                     base::Unretained(this)));
}

void BrailleControllerImpl::OnSocketDirChangedOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VLOG(1) << "BrlAPI directory changed";
  // Every directory change resets the max retry time to the appropriate delay
  // into the future.
  ResetRetryConnectHorizon();
  // Try after an initial delay to give the driver a chance to connect.
  ScheduleTryToConnect();
}

void BrailleControllerImpl::TryToConnect() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(skip_libbrlapi_so_load_ || libbrlapi_loader_.loaded());
  connect_scheduled_ = false;
  if (!connection_.get()) {
    DCHECK(!create_brlapi_connection_function_.is_null());
    connection_ = std::move(create_brlapi_connection_function_).Run();
  }

  DCHECK(connection_);
  if (!connection_->Connected()) {
    VLOG(1) << "Trying to connect to brlapi";
    BrlapiConnection::ConnectResult result =
        connection_->Connect(base::BindRepeating(
            &BrailleControllerImpl::DispatchKeys, base::Unretained(this)));
    switch (result) {
      case BrlapiConnection::CONNECT_SUCCESS:
        DispatchOnDisplayStateChanged(GetDisplayState());
        break;
      case BrlapiConnection::CONNECT_ERROR_NO_RETRY:
        break;
      case BrlapiConnection::CONNECT_ERROR_RETRY:
        ScheduleTryToConnect();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }
}

void BrailleControllerImpl::ResetRetryConnectHorizon() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  retry_connect_horizon_ = base::Time::Now() + kConnectRetryTimeout;
}

void BrailleControllerImpl::ScheduleTryToConnect() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Don't reschedule if there's already a connect scheduled or
  // the next attempt would fall outside of the retry limit.
  if (connect_scheduled_)
    return;
  if (base::Time::Now() + kConnectionDelay > retry_connect_horizon_) {
    VLOG(1) << "Stopping to retry to connect to brlapi";
    return;
  }
  VLOG(1) << "Scheduling connection retry to brlapi";
  connect_scheduled_ = true;
  content::GetIOThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BrailleControllerImpl::TryToConnect,
                     base::Unretained(this)),
      kConnectionDelay);
}

void BrailleControllerImpl::Disconnect() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!connection_ || !connection_->Connected())
    return;
  connection_->Disconnect();
  DispatchOnDisplayStateChanged(std::make_unique<DisplayState>());
}

std::unique_ptr<BrlapiConnection>
BrailleControllerImpl::CreateBrlapiConnection() {
  DCHECK(skip_libbrlapi_so_load_ || libbrlapi_loader_.loaded());
  return BrlapiConnection::Create(&libbrlapi_loader_);
}

void BrailleControllerImpl::DispatchKeys() {
  DCHECK(connection_.get());
  brlapi_keyCode_t code;
  while (true) {
    int result = connection_->ReadKey(&code);
    if (result < 0) {  // Error.
      brlapi_error_t* err = connection_->BrlapiError();
      if (err->brlerrno == BRLAPI_ERROR_LIBCERR && err->libcerrno == EINTR)
        continue;
      // Disconnect on other errors.
      VLOG(1) << "BrlAPI error: " << connection_->BrlapiStrError();
      Disconnect();
      return;
    } else if (result == 0) {  // No more data.
      return;
    }
    std::unique_ptr<KeyEvent> event = BrlapiKeyCodeToEvent(code);
    if (event)
      DispatchKeyEvent(std::move(event));
  }
}

void BrailleControllerImpl::DispatchKeyEvent(std::unique_ptr<KeyEvent> event) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&BrailleControllerImpl::DispatchKeyEvent,
                                  base::Unretained(this), std::move(event)));
    return;
  }
  VLOG(1) << "Dispatching key event: " << event->ToValue();
  for (auto& observer : observers_)
    observer.OnBrailleKeyEvent(*event);
}

void BrailleControllerImpl::DispatchOnDisplayStateChanged(
    std::unique_ptr<DisplayState> new_state) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    if (!content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(
                &BrailleControllerImpl::DispatchOnDisplayStateChanged,
                base::Unretained(this), std::move(new_state)))) {
      NOTREACHED_IN_MIGRATION();
    }
    return;
  }
  for (auto& observer : observers_)
    observer.OnBrailleDisplayStateChanged(*new_state);
}

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions

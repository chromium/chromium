/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-present eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "chrome/browser/android/adblock/adblock_bridge.h"

#include <jni.h>
#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"

#include "AdblockPlus.h"
#include "jni/AdblockBridge_jni.h"

#include "gin/public/isolate_holder.h"
#include "gin/v8_initializer.h"
#include "content/public/browser/browser_thread.h"
#include "base/task_scheduler/post_task.h"
#include "base/synchronization/condition_variable.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_isolated_world_ids.h"

// prefs
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_member.h"
#include "chrome/common/pref_names.h"
#include "chrome/browser/profiles/profile.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ScopedJavaGlobalRef;

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

bool AdblockBridge::prefs_moved_to_thread = false;
base::LazyInstance<scoped_refptr<base::SingleThreadTaskRunner>>::DestructorAtExit
  task_runner = LAZY_INSTANCE_INITIALIZER;

std::string replaceString(
  std::string subject,
  const std::string& search,
  const std::string& replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
      subject.replace(pos, search.length(), replace);
      pos += replace.length();
    }
    return subject;
}

std::string escapeSelector(std::string selector) {
  return replaceString(replaceString(selector, "\\", "\\\\"), "\"", "\\\"");
}

std::string generateJavascript(
  AdblockPlus::FilterEnginePtr filterEngine,
  std::string url,
  std::string domain) {
  LOG(WARNING) << "Adblock: getting selectors for domain " << domain;
  std::vector<std::string> selectors = filterEngine->GetElementHidingSelectors(domain);
  LOG(WARNING) << "Adblock: got " << selectors.size() << " selectors for domain " << domain;

  // build the string with selectors
  std::stringstream ss;
  ss << "var selectors = [\n";
  for (unsigned int i=0; i<selectors.size(); i++) {
    if (i > 0) {
      ss << ", ";
    }
    //LOG(WARNING) << "Adblock: selector: " << selectors[i];
    ss << "\"" << escapeSelector(selectors[i]) << "\"\n";
  }
  ss << "]\n";

  ss <<
"console.log('parsed selectors: ' + selectors.length);\n\
var head = document.getElementsByTagName(\"head\")[0];\n\
var style = document.createElement(\"style\");\n\
head.appendChild(style);\n\
var sheet = style.sheet ? style.sheet : style.styleSheet;\n\
for (var i=0; i<selectors.length; i++)\n\
{\n\
 if (sheet.insertRule)\n\
 {\n\
   sheet.insertRule(selectors[i] + ' { display: none !important; }', 0);\n\
 }\n\
 else\n\
 {\n\
   sheet.addRule(selectors[i], 'display: none !important;', 0);\n\
 }\n\
}\n\
console.log('finished injecting css rules');";

  return ss.str();
}

void handleOnLoad(content::WebContents* webContents, int frameTreeNodeId) {
  LOG(WARNING) << "Adblock: handleOnLoad()";
  // it can be released in UI thread while this is not yet invoked
  if (!AdblockBridge::enable_adblock ||
      !AdblockBridge::getFilterEnginePtr() ||
      !AdblockBridge::adblock_whitelisted_domains) {
    LOG(WARNING) << "Adblock: !something in handleOnLoad()";
    return;
  }

  if (!AdblockBridge::enable_adblock->GetValue()) {
    LOG(WARNING) << "Adblock: adblocking is disabled, skip apply element hiding";
    return;
  }

  // retain local filter engine to prevent usage of released instance if it's released on android/java side
  AdblockPlus::FilterEnginePtr* extFilterEngine =
    reinterpret_cast<AdblockPlus::FilterEnginePtr*>(AdblockBridge::getFilterEnginePtr());
  AdblockPlus::FilterEnginePtr filterEngine(*extFilterEngine);

  LOG(WARNING) << "Adblock: element hiding: casted to AdblockPlus::FilterEnginePtr, "
               << "use_count = " << filterEngine.use_count();

  GURL gurl = webContents->GetURL();
  std::string url = gurl.spec();

  std::vector<std::string> referrers;
  referrers.push_back(url);

  // user domains whitelisting is implemented as adding exception filter for domain
  // so we can just use `filterEngine.is...Whitelisted()`
  if (gurl.SchemeIsHTTPOrHTTPS()) {
    if (filterEngine->IsDocumentWhitelisted(url, referrers) ||
        filterEngine->IsElemhideWhitelisted(url, referrers)) {
       LOG(WARNING) << "Adblock: element hiding - whitelisted";
    } else {
      // generate JS
      LOG(WARNING) << "Adblock: element hiding - generating JS ...";
      std::string domain = filterEngine->GetHostFromURL(url);
      std::string js = generateJavascript(filterEngine, url, domain);
      LOG(WARNING) << "Adblock: element hiding - generated JS";

      // run JS
      content::RenderFrameHost* frameHost = (frameTreeNodeId
        ? webContents->UnsafeFindFrameByFrameTreeNodeId(frameTreeNodeId)
        : webContents->GetMainFrame());

      if (frameHost) {
        frameHost->ExecuteJavaScriptInIsolatedWorld(
          base::UTF8ToUTF16(js),
          content::RenderFrameHost::JavaScriptResultCallback(),
          ISOLATED_WORLD_ID_ADBLOCK);

        LOG(WARNING) << "Adblock: element hiding - called JS in frame"
                     << " '" << frameHost->GetFrameName() << "'";
      } else {
        LOG(ERROR) << "Adblock: failed to find frameHost for frameTreeNodeId " << frameTreeNodeId;
      }
    }
  } 
}

void ReleaseIsolateHolder(gin::IsolateHolder* isolateHolder) {
  delete isolateHolder;
  LOG(WARNING) << "Deleted IsolateHolder";
}

class IsolateHolderV8Provider : public AdblockPlus::IV8IsolateProvider  
{  
  public:  
    IsolateHolderV8Provider(gin::IsolateHolder* isolateHolder_)
      : isolateHolder(isolateHolder_)
    {  
    }  
  
    v8::Isolate* Get() override
    {  
      return isolateHolder->isolate();  
    }

    ~IsolateHolderV8Provider() override
    {
      LOG(WARNING) << "Posting 'delete isolateHolder' task";
      task_runner.Get()->PostTask(FROM_HERE, base::BindOnce(&ReleaseIsolateHolder, isolateHolder));
    }
  
  private:
    IsolateHolderV8Provider(const IsolateHolderV8Provider&);  
    IsolateHolderV8Provider& operator=(const IsolateHolderV8Provider&);

    gin::IsolateHolder* isolateHolder;  
};

class AdblockLoadCompleteListener : public content::NotificationObserver {
  public:
    explicit AdblockLoadCompleteListener(
      bool subscribeToDidFinishNavigation,
      bool subscribeToMainFrameCompleted) :
        subscribeToDidFinishNavigation_(subscribeToDidFinishNavigation),
        subscribeToMainFrameCompleted_(subscribeToMainFrameCompleted)
    {
      if (subscribeToDidFinishNavigation_) {
        registrar_.Add(this, content::NOTIFICATION_DID_FINISH_NAVIGATION,
                       content::NotificationService::AllSources());
      }

      if (subscribeToMainFrameCompleted_) {
        registrar_.Add(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                       content::NotificationService::AllSources());
      }
    }

    ~AdblockLoadCompleteListener() override {
      if (subscribeToDidFinishNavigation_ && registrar_.IsRegistered(this,
            content::NOTIFICATION_DID_FINISH_NAVIGATION,
            content::NotificationService::AllSources())) {
        registrar_.Remove(this, content::NOTIFICATION_DID_FINISH_NAVIGATION,
                          content::NotificationService::AllSources());
      }

      if (subscribeToMainFrameCompleted_ && registrar_.IsRegistered(this,
            content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
            content::NotificationService::AllSources())) {
        registrar_.Remove(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                          content::NotificationService::AllSources());
      }
    }

  private:
    content::NotificationRegistrar registrar_;
    bool subscribeToDidFinishNavigation_;
    bool subscribeToMainFrameCompleted_;

    // content::NotificationObserver:
    void Observe(int type,
                 const content::NotificationSource& source,
                 const content::NotificationDetails& details) override {
      content::WebContents* webContents = content::Source<content::WebContents>(source).ptr();
      int frameTreeNodeId = (int)content::Details<int>(details).ptr();

      LOG(WARNING) << "Adblock: received onLoad() notification of type " << type
                   << " with url " << webContents->GetURL().spec()
                   << " and frame node id " << frameTreeNodeId;
      
      if (!AdblockBridge::getFilterEnginePtr()) {
        LOG(WARNING) << "Adblock: inject JS skipped (no filter engine)";
        return;
      }

      // run in (background) thread in order not to block main (UI) thread for few seconds
      // prefs can be `nullptr` only if they are released 
      if (!task_runner.Get() ||
          !AdblockBridge::enable_adblock ||
          !AdblockBridge::adblock_whitelisted_domains) {
          LOG(WARNING) << "Adblock: !something, exiting elemhide";
        return;
      }

      // prefs should be moved to the thread they will be accessed from
      // (should be invoked from UI thread!)
      if (!AdblockBridge::prefs_moved_to_thread) {
        LOG(WARNING) << "Adblock: moving elemehide prefs to background thread";

        AdblockBridge::enable_adblock->MoveToThread(task_runner.Get());
        AdblockBridge::adblock_whitelisted_domains->MoveToThread(task_runner.Get());

        AdblockBridge::prefs_moved_to_thread = true;
      }

      // run in (background) thread in order not to block main (UI) thread for few seconds
      task_runner.Get()->PostTask(FROM_HERE, base::BindOnce(&handleOnLoad, webContents, frameTreeNodeId));
    }

    DISALLOW_COPY_AND_ASSIGN(AdblockLoadCompleteListener);
};

// ----------------------------------------------------------------------------

base::LazyInstance<base::Lock>::DestructorAtExit AdblockBridge::filterEnginePtrLock;
jlong AdblockBridge::filterEnginePtr = 0;

jlong AdblockBridge::getFilterEnginePtr()
{
  const base::AutoLock auto_lock(filterEnginePtrLock.Get());
  return filterEnginePtr;
}

void AdblockBridge::setFilterEnginePtr(jlong ptr)
{
  const base::AutoLock auto_lock(filterEnginePtrLock.Get());
  filterEnginePtr = ptr;
}

BooleanPrefMember* AdblockBridge::enable_adblock = nullptr;
StringListPrefMember* AdblockBridge::adblock_whitelisted_domains = nullptr;
AdblockLoadCompleteListener* completeListener = nullptr;

void AdblockBridge::InitializePrefsOnUIThread(PrefService* pref_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  LOG(WARNING) << "Adblock: init prefs for element hiding";
  
  enable_adblock = new BooleanPrefMember();
  enable_adblock->Init(prefs::kEnableAdblock, pref_service);

  adblock_whitelisted_domains = new StringListPrefMember();
  adblock_whitelisted_domains->Init(prefs::kAdblockWhitelistedDomains, pref_service);
}

void AdblockBridge::ReleasePrefs() {
  prefs_moved_to_thread = false;
  
  enable_adblock->Destroy();
  delete enable_adblock;
  enable_adblock = nullptr;

  adblock_whitelisted_domains->Destroy();
  delete adblock_whitelisted_domains;
  adblock_whitelisted_domains = nullptr;
}

static void SubscribeOnLoadListener() {
  // "DidFinishNavigation" seems to be enough
  completeListener = new AdblockLoadCompleteListener(true, false);
}

static void UnsubscribeOnLoadListener() {
  delete completeListener;
  completeListener = nullptr;
}


void ReleaseTaskRunner() {
  LOG(WARNING) << "Adblock: releasing task runner";
  task_runner.Get() = nullptr;
}


void ReleaseAdblock() {
  LOG(WARNING) << "Adblock: releasing everything";
  ReleaseTaskRunner();
}

static void JNI_AdblockBridge_SetFilterEngineNativePtr(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jlong ptr)
{  
  LOG(WARNING) << "Adblock: set FilterEngine instance " << ptr;
  jlong prevPtr = AdblockBridge::getFilterEnginePtr();
  AdblockBridge::setFilterEnginePtr(ptr);

  // if we had not filter engine and now it's available 
  if (!prevPtr && ptr) {
    // start receive notifications to apply element hiding
    SubscribeOnLoadListener();
  }
  // if we had filter engine and now it's no longer available
  else if (prevPtr && !ptr)
  {
    LOG(WARNING) << "Adblock: schedule release on IO thread";

    // this should be invoked on UI thread
    UnsubscribeOnLoadListener();
    AdblockBridge::ReleasePrefs();

    // we have to run it on IO thread (not UI thread)
    // as othwrwise thread->Stop() will crash with:
    // FATAL:thread_restrictions.cc(38)] Check failed: false.
    // Function marked as IO-only was called from a thread that disallows IO!
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner = 
      base::CreateSingleThreadTaskRunnerWithTraits(
        {base::MayBlock(), base::TaskPriority::BACKGROUND});

    io_task_runner->PostTask(FROM_HERE, base::BindOnce(&ReleaseAdblock));
  }
}

class AdblockIsolationProviderCompositor
    : public base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope {};

void createIsolateProviderOnBgThread(
  base::Lock* lock,
  base::ConditionVariable* event,
  AdblockPlus::IV8IsolateProvider** ptr) {
  LOG(INFO) << "Adblock: createIsolateProviderOnBgThread() started";

  base::AutoLock auto_lock(*lock);

  // create isolate using isolate holder (using kUseLocker!)
  *ptr = new IsolateHolderV8Provider(
      new gin::IsolateHolder(
        task_runner.Get(),
        gin::IsolateHolder::AccessMode::kUseLocker));

  event->Signal();
  LOG(INFO) << "Adblock: createIsolateProviderOnBgThread() finished";
}

static jlong JNI_AdblockBridge_GetIsolateProviderNativePtr(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller)
{
  // v8 init
  LOG(WARNING) << "Adblock: creating isolate holder ...";
  
  #ifdef V8_USE_EXTERNAL_STARTUP_DATA
  LOG(WARNING) << "Adblock: loading v8 snapshot & natives ...";
  gin::V8Initializer::LoadV8Snapshot();
  gin::V8Initializer::LoadV8Natives();
  LOG(WARNING) << "Adblock: loaded v8 snapshot & natives";
  #endif

  LOG(WARNING) << "Adblock: initialize isolate holder";
  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::IsolateHolder::kStableV8Extras,
                                 gin::ArrayBufferAllocator::SharedInstance());

  task_runner.Get() = base::CreateSingleThreadTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);

  AdblockPlus::IV8IsolateProvider* isolateProviderPtr = nullptr;

  // post to bg thread and wait for isolateProvider to be created
  base::Lock lock;
  base::ConditionVariable event(&lock);
  base::AutoLock auto_lock(lock);

  task_runner.Get()->PostTask(FROM_HERE,
                              base::BindOnce(
                                &createIsolateProviderOnBgThread,
                                &lock, &event, &isolateProviderPtr));
  {
    AdblockIsolationProviderCompositor allow_wait;
    LOG(WARNING) << "Adblock: waiting isolate provider created";
    event.Wait();
    LOG(WARNING) << "Adblock: got isolate provider created event";
  }

  // return isolate pointer
  LOG(WARNING) << "Adblock: returning (from adblock_bridge.cc) isolate provider " << isolateProviderPtr;
  return (jlong)isolateProviderPtr;
}
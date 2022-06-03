// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/bho/bho.h"

#include <atlstr.h>
#include <initguid.h>
#include <time.h>
#include <wininet.h>

#include "chrome/browser/browser_switcher/bho/logging.h"

// Declare this GUID for Windows 8 Enhanced Protected Mode Compatibility.
// This is defined in newer SDK versions but since we need XP support we can't
// use them.
DEFINE_GUID(CATID_AppContainerCompatible,
            0x59fb2056,
            0xd625,
            0x48d0,
            0xa9,
            0x44,
            0x1a,
            0x85,
            0xb5,
            0xab,
            0x26,
            0x40);

namespace {
const wchar_t kHttpPrefix[] = L"http://";
const wchar_t kHttpsPrefix[] = L"https://";
const wchar_t kFilePrefix[] = L"file://";
}  // namespace

CBrowserSwitcherBHO::CBrowserSwitcherBHO() = default;

CBrowserSwitcherBHO::~CBrowserSwitcherBHO() = default;

// Implementation of IObjectWithSiteImpl::SetSite.
STDMETHODIMP CBrowserSwitcherBHO::SetSite(IUnknown* site) noexcept {
  if (site != NULL) {
    HRESULT hr = site->QueryInterface(IID_PPV_ARGS(&web_browser_));
    if (SUCCEEDED(hr)) {
      hr = DispEventAdvise(web_browser_.Get());
      advised_ = true;
    }
  } else {  // site == NULL
    if (advised_) {
      DispEventUnadvise(web_browser_.Get());
      advised_ = false;
    }
    web_browser_.Reset();
  }
  return IObjectWithSiteImpl<CBrowserSwitcherBHO>::SetSite(site);
}

// If enabled,  navigations and redirects them to Chrome if they are
// not intended to happen in IE according to the Legacy Browser Support policy.
// This only applies to top-level documents (not to frames).
void STDMETHODCALLTYPE
CBrowserSwitcherBHO::BeforeNavigate(IDispatch* disp,
                                    VARIANT* url,
                                    VARIANT* flags,
                                    VARIANT* target_frame_name,
                                    VARIANT* post_data,
                                    VARIANT* headers,
                                    VARIANT_BOOL* cancel) {
  if (web_browser_ != NULL && disp != NULL) {
    Microsoft::WRL::ComPtr<IUnknown> unknown1;
    Microsoft::WRL::ComPtr<IUnknown> unknown2;
    if (SUCCEEDED(web_browser_.As(&unknown1)) &&
        SUCCEEDED(disp->QueryInterface(IID_PPV_ARGS(&unknown2)))) {
      // check if this is the outer frame.
      if (unknown1 == unknown2) {
        bool result =
            CheckUrl((LPOLESTR)url->bstrVal, *cancel != VARIANT_FALSE);
        if (result)
          web_browser_->Quit();
        *cancel = result ? VARIANT_TRUE : VARIANT_FALSE;
      }
    }
  }
}

// Checks if an url should be loaded in IE or forwarded to the default browser.
bool CBrowserSwitcherBHO::CheckUrl(LPOLESTR url, bool cancel) {
  // Only verify the url if it is http[s] link.
  if ((!_wcsnicmp(url, kHttpPrefix, wcslen(kHttpPrefix)) ||
       !_wcsnicmp(url, kHttpsPrefix, wcslen(kHttpsPrefix)) ||
       !_wcsnicmp(url, kFilePrefix, wcslen(kFilePrefix))) &&
      !browser_switcher_.ShouldOpenInAlternativeBrowser(url)) {
    LOG(INFO) << "\tTriggering redirect" << std::endl;
    if (!browser_switcher_.InvokeChrome(url)) {
      LOG(ERR) << "Could not invoke alternative browser! "
               << "Will resume loading in IE!" << std::endl;
    } else {
      cancel = true;
    }
  }
  return cancel;
}

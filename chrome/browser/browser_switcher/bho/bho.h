// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BHO_BHO_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BHO_BHO_H_

#include "base/win/atl.h"

#include <exdispid.h>  // NOLINT(build/include_order)
#include <shlguid.h>   // NOLINT(build/include_order)
#include <shlobj.h>    // NOLINT(build/include_order)
#include <wrl/client.h>

#include "chrome/browser/browser_switcher/bho/browser_switcher_core.h"
#include "chrome/browser/browser_switcher/bho/ie_bho_idl.h"
#include "chrome/browser/browser_switcher/bho/resource.h"

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

// Once loaded, this BHO ensures that any url that is not meant for IE is opened
// in the default browser.
class ATL_NO_VTABLE CBrowserSwitcherBHO
    : public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>,
      public ATL::CComCoClass<CBrowserSwitcherBHO, &CLSID_BrowserSwitcherBHO>,
      public ATL::IObjectWithSiteImpl<CBrowserSwitcherBHO>,
      public ATL::IDispatchImpl<IBrowserSwitcherBHO,
                                &IID_IBrowserSwitcherBHO,
                                &LIBID_BrowserSwitcherLib,
                                1,
                                0>,
      public ATL::IDispEventImpl<1,
                                 CBrowserSwitcherBHO,
                                 &DIID_DWebBrowserEvents2,
                                 &LIBID_SHDocVw,
                                 1,
                                 1> {
 public:
  CBrowserSwitcherBHO();
  ~CBrowserSwitcherBHO() override;

  STDMETHOD(SetSite)(IUnknown* site);

  DECLARE_REGISTRY_RESOURCEID(IDR_BROWSERSWITCHERBHO)

  BEGIN_CATEGORY_MAP(CBrowserSwitcherBHO)
  IMPLEMENTED_CATEGORY(CATID_AppContainerCompatible)
  END_CATEGORY_MAP()

  BEGIN_COM_MAP(CBrowserSwitcherBHO)
  COM_INTERFACE_ENTRY(IBrowserSwitcherBHO)
  COM_INTERFACE_ENTRY(IDispatch)
  COM_INTERFACE_ENTRY(IObjectWithSite)
  END_COM_MAP()

  BEGIN_SINK_MAP(CBrowserSwitcherBHO)
  SINK_ENTRY_EX(1,
                DIID_DWebBrowserEvents2,
                DISPID_BEFORENAVIGATE2,
                &CBrowserSwitcherBHO::BeforeNavigate)
  END_SINK_MAP()

  void STDMETHODCALLTYPE BeforeNavigate(IDispatch* disp,
                                        VARIANT* url,
                                        VARIANT* flags,
                                        VARIANT* target_frame_name,
                                        VARIANT* post_data,
                                        VARIANT* headers,
                                        VARIANT_BOOL* cancel);

 private:
  bool CheckUrl(LPOLESTR url, bool cancel);

  Microsoft::WRL::ComPtr<IWebBrowser2> web_browser_;
  bool advised_;

  BrowserSwitcherCore browser_switcher_;
};

// OBJECT_ENTRY_AUTO() contains an extra semicolon that causes compilation to
// fail.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi"
#endif

OBJECT_ENTRY_AUTO(__uuidof(BrowserSwitcherBHO), CBrowserSwitcherBHO)

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BHO_BHO_H_

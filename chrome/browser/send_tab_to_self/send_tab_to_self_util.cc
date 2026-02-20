// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include <iterator>
#include <optional>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/send_tab_to_self/outgoing_tab_form_field_extractor.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/received_tab_forms_filler.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace send_tab_to_self {

std::optional<EntryPointDisplayReason> GetEntryPointDisplayReason(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return std::nullopt;
  }

  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  return service ? service->GetEntryPointDisplayReason(
                       web_contents->GetLastCommittedURL())
                 : std::nullopt;
}

bool ShouldDisplayEntryPoint(content::WebContents* web_contents) {
  return GetEntryPointDisplayReason(web_contents).has_value();
}

PageContext ExtractFormFieldsFromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return PageContext();
  }

  const url::Origin main_origin =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  PageContext context;

  web_contents->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
    autofill::ContentAutofillDriver* driver =
        autofill::ContentAutofillDriver::GetForRenderFrameHost(rfh);
    if (!driver) {
      return;
    }

    PageContext::FormFieldInfo frame_info =
        ExtractOutgoingTabFormFields(driver->GetAutofillManager(), main_origin);
    context.form_field_info.fields.insert(
        context.form_field_info.fields.end(),
        std::make_move_iterator(frame_info.fields.begin()),
        std::make_move_iterator(frame_info.fields.end()));
  });

  return context;
}

void FillWebContents(content::WebContents* web_contents,
                     const url::Origin& origin,
                     const PageContext& page_context) {
  if (!web_contents || page_context.form_field_info.fields.empty()) {
    return;
  }

  autofill::ContentAutofillClient* autofill_client =
      autofill::ContentAutofillClient::FromWebContents(web_contents);
  if (autofill_client) {
    ReceivedTabFormsFiller::Start(*autofill_client, origin,
                                  page_context.form_field_info);
  }
}

}  // namespace send_tab_to_self

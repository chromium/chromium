// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/default_ranking.h"

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"

namespace sharing {

namespace {

// All four of these rankings are sourced from a combination of public app usage
// data sets and Chrome-side metrics about common share targets; the exact
// details of that process are described in go/sh-plus.
//
// These are lists of activities rather than packages, so that multiple share
// targets from the same app can appear in different places in the ranking. This
// is a bit brittle; see https://crbug.com/1222156#c4 for why. Regrettably, the
// mapping from human-readable (app name, action) pairs to activities was done
// by hand; see go/sh-3p-activities for that.
//
// These lists are used to rank which share targets are shown for users with no
// share history. In all cases, only apps actually present on the device are
// shown; these lists control the relative ordering when many of these apps are
// all present at once. Once the user has any share history, ShareRanking will
// use that history rather than these default lists.
//
// You'll notice that this data is split up by "EnUs" and "World"; this is
// because the popularity of apps around the world in general is pretty similar,
// *except* for en-US users, who have pretty major differences. If other locales
// diverge more from the main world list we will need to add more cases here for
// them.

struct ComponentName {
  ComponentName(const std::string& package, const std::string& activity)
      : package(package), activity(activity) {}

  std::string package;
  std::string activity;

  // Note: the format of this string must match that returned by
  // android.content.ComponentName#flattenToString
  std::string Flatten() const { return package + "/" + activity; }
};

std::vector<ComponentName> DefaultEnUsImageRanking() {
  return {
      {"com.whatsapp", "com.whatsapp.ContactPicker"},
      {"com.google.android.apps.messaging",
       "com.google.android.apps.messaging.ui.conversationlist."
       "ShareIntentActivity"},
      {"com.google.android.gm",
       "com.google.android.gm.ComposeActivityGmailExternal"},
      {"com.facebook.orca",
       "com.facebook.messenger.intents.ShareIntentHandler"},
      {"com.facebook.katana",
       "com.facebook.composer.shareintent."
       "ImplicitShareIntentHandlerDefaultAlias"},
      {"com.google.android.apps.photos",
       "com.google.android.apps.photos.uploadtoalbum.UploadContentActivity"},
      // TODO(crbug.com/40777590): Files
      {
          "com.snapchat.android",
          "com.snap.mushroom.MainActivity",
      },
      {
          "org.telegram.messenger",
          "org.telegram.ui.LaunchActivity",
      },
      {"com.instagram.android",
       "com.instagram.share.handleractivity.StoryShareHandlerActivity"},
      {"com.instagram.android",
       "com.instagram.share.handleractivity.ShareHandlerActivity"},
      {"com.discord", "com.discord.app.AppActivity$AppAction"},
      {"com.instagram.android",
       "com.instagram.direct.share.handler."
       "DirectExternalMediaShareActivityPhoto"},
      {
          "com.google.android.talk",
          "com.google.android.apps.hangouts.phone.ShareIntentActivity",
      },
      {
          "com.google.android.keep",
          "com.google.android.keep.activities.ShareReceiverActivity",
      },
      {
          "com.google.android.apps.docs.editors.docs",
          "com.google.android.apps.docs.common.shareitem.UploadMenuActivity",
      },
      {
          "com.verizon.messaging.vzmsgs",
          "com.verizon.mms.ui.LaunchConversationActivity",
      },
      {
          "org.thoughtcrime.securesms",
          "org.thoughtcrime.securesms.sharing.ShareActivity",
      },
      {
          "com.twitter.android",
          "com.twitter.composer.ComposerActivity",
      },
      {
          "com.pinterest",
          "com.pinterest.activity.create.PinItActivity",
      },
      {
          "com.twitter.android",
          "com.twitter.app.dm.DMActivity",
      },
      {
          "com.google.android.apps.dynamite",
          "com.google.android.apps.dynamite.activity.main.MainActivity",
      },
      {
          "com.microsoft.office.outlook",
          "com.microsoft.office.outlook.compose.ComposeLauncherActivity",
      },
      {
          "com.yahoo.mobile.client.android.mail",
          "com.yahoo.mail.flux.ui.MailComposeActivity",
      },
      {
          "com.linkedin.android",
          "com.linkedin.android.publishing.sharing.SharingDeepLinkActivity",
      },
      // TODO(crbug.com/40777590): Samsung email
      {
          "com.reddit.frontpage",
          "com.reddit.sharing.ShareActivity",
      },
      {
          "jp.naver.line.android",
          "com.linecorp.line.share.common.view.FullPickerLaunchActivity",
      },
      {
          "com.tencent.mm",
          "com.tencent.mm.ui.tools.ShareImgUI",
      },
      {
          "com.tencent.mm",
          "com.tencent.mm.ui.tools.ShareToTimeLineUI",
      },
      // TODO(crbug.com/40777590): Groupme
  };
}

std::vector<ComponentName> DefaultEnUsTextRanking() {
  return {
      {"com.whatsapp", "com.whatsapp.ContactPicker"},
      {"com.google.android.apps.messaging",
       "com.google.android.apps.messaging.ui.conversationlist."
       "ShareIntentActivity"},
      {"com.google.android.gm",
       "com.google.android.gm.ComposeActivityGmailExternal"},
      {"com.facebook.orca",
       "com.facebook.messenger.intents.ShareIntentHandler"},
      {"com.facebook.katana",
       "com.facebook.composer.shareintent."
       "ImplicitShareIntentHandlerDefaultAlias"},
      {
          "com.snapchat.android",
          "com.snap.mushroom.MainActivity",
      },
      {
          "org.telegram.messenger",
          "org.telegram.ui.LaunchActivity",
      },
      {"com.discord", "com.discord.app.AppActivity$AppAction"},
      {"com.instagram.android",
       "com.instagram.direct.share.handler."
       "DirectExternalMediaShareActivityPhoto"},
      {
          "com.google.android.talk",
          "com.google.android.apps.hangouts.phone.ShareIntentActivity",
      },
      {
          "com.google.android.keep",
          "com.google.android.keep.activities.ShareReceiverActivity",
      },
      {
          "com.verizon.messaging.vzmsgs",
          "com.verizon.mms.ui.LaunchConversationActivity",
      },
      {
          "org.thoughtcrime.securesms",
          "org.thoughtcrime.securesms.sharing.ShareActivity",
      },
      {
          "com.twitter.android",
          "com.twitter.composer.ComposerActivity",
      },
      {
          "com.pinterest",
          "com.pinterest.activity.create.PinItActivity",
      },
      {
          "com.twitter.android",
          "com.twitter.app.dm.DMActivity",
      },
      {
          "com.ideashower.readitlater.pro",
          "com.ideashower.readitlater.activity.AddActivity",
      },
      {
          "com.google.android.apps.dynamite",
          "com.google.android.apps.dynamite.activity.main.MainActivity",
      },
      {
          "com.google.android.apps.docs.editors.docs",
          "com.google.android.apps.docs.common.shareitem.UploadMenuActivity",
      },
      {
          "com.microsoft.office.outlook",
          "com.microsoft.office.outlook.compose.ComposeLauncherActivity",
      },
      {
          "com.yahoo.mobile.client.android.mail",
          "com.yahoo.mail.flux.ui.MailComposeActivity",
      },
      {
          "com.linkedin.android",
          "com.linkedin.android.publishing.sharing.SharingDeepLinkActivity",
      },
      // TODO(crbug.com/40777590): Samsung email
      {
          "com.reddit.frontpage",
          "com.reddit.sharing.ShareActivity",
      },
      {
          "jp.naver.line.android",
          "com.linecorp.line.share.common.view.FullPickerLaunchActivity",
      },
      {
          "com.tencent.mm",
          "com.tencent.mm.ui.tools.ShareImgUI",
      },
      {
          "com.tencent.mm",
          "com.tencent.mm.ui.tools.ShareToTimeLineUI",
      },
      // TODO(crbug.com/40777590): Groupme
  };
}

std::vector<ComponentName> DefaultWorldImageRanking() {
  return {
      {"com.whatsapp", "com.whatsapp.ContactPicker"},
      {"com.google.android.apps.messaging",
       "com.google.android.apps.messaging.ui.conversationlist."
       "ShareIntentActivity"},
      {"com.facebook.orca",
       "com.facebook.messenger.intents.ShareIntentHandler"},
      {"com.google.android.gm",
       "com.google.android.gm.ComposeActivityGmailExternal"},
      {"com.instagram.android",
       "com.instagram.share.handleractivity.ShareHandlerActivity"},
      {"com.instagram.android",
       "com.instagram.share.handleractivity.StoryShareHandlerActivity"},
      {
          "com.tencent.mm",
          "com.tencent.mm.ui.tools.ShareToTimeLineUI",
      },
      {
          "org.telegram.messenger",
          "org.telegram.ui.LaunchActivity",
      },
      {"com.facebook.katana",
       "com.facebook.composer.shareintent."
       "ImplicitShareIntentHandlerDefaultAlias"},
      {
          "jp.naver.line.android",
          "com.linecorp.line.share.common.view.FullPickerLaunchActivity",
      },
      {
          "com.google.android.talk",
          "com.google.android.apps.hangouts.phone.ShareIntentActivity",
      },
      {"com.google.android.apps.photos",
       "com.google.android.apps.photos.uploadtoalbum.UploadContentActivity"},
      // TODO(crbug.com/40777590): Files
      {
          "com.google.android.apps.docs.editors.docs",
          "com.google.android.apps.docs.common.shareitem.UploadMenuActivity",
      },
      {"com.instagram.android",
       "com.instagram.direct.share.handler."
       "DirectExternalMediaShareActivityPhoto"},
      {
          "com.google.android.keep",
          "com.google.android.keep.activities.ShareReceiverActivity",
      },
      {
          "com.snapchat.android",
          "com.snap.mushroom.MainActivity",
      },
      {"com.discord", "com.discord.app.AppActivity$AppAction"},
      {
          "com.twitter.android",
          "com.twitter.composer.ComposerActivity",
      },
      // TODO(crbug.com/40777253): Whatsapp Business
      {
          "com.pinterest",
          "com.pinterest.activity.create.PinItActivity",
      },
      {
          "com.linkedin.android",
          "com.linkedin.android.publishing.sharing.SharingDeepLinkActivity",
      },
      {
          "com.facebook.mlite",
          "com.facebook.mlite.share.view.ShareActivity",
      },
      {
          "org.thoughtcrime.securesms",
          "org.thoughtcrime.securesms.sharing.ShareActivity",
      },
      {
          "com.twitter.android",
          "com.twitter.app.dm.DMActivity",
      },
      {
          "com.facebook.lite",
          "com.facebook.lite.stories.activities.ShareToFbStoriesAlias",
      },
      {
          "com.microsoft.office.outlook",
          "com.microsoft.office.outlook.compose.ComposeLauncherActivity",
      },
      {
          "com.yahoo.mobile.client.android.mail",
          "com.yahoo.mail.flux.ui.MailComposeActivity",
      },
      {
          "com.viber.voip",
          "com.viber.voip.WelcomeShareActivity",
      },
      {
          "com.imo.android.imoim",
          "com.imo.android.imoim.globalshare.SharingActivity2",
      },
      // TODO(crbug.com/40777590): Samsung email
      {
          "com.tencent.mm",
          "com.tencent.mm.ui.tools.ShareImgUI",
      },
      {
          "com.verizon.messaging.vzmsgs",
          "com.verizon.mms.ui.LaunchConversationActivity",
      },
      {
          "com.google.android.apps.dynamite",
          "com.google.android.apps.dynamite.activity.main.MainActivity",
      },
  };
}

std::vector<ComponentName> DefaultWorldTextRanking() {
  return {
      {"com.whatsapp", "com.whatsapp.ContactPicker"},
      {"com.google.android.apps.messaging",
       "com.google.android.apps.messaging.ui.conversationlist."
       "ShareIntentActivity"},
      {"com.facebook.orca",
       "com.facebook.messenger.intents.ShareIntentHandler"},
      {"com.google.android.gm",
       "com.google.android.gm.ComposeActivityGmailExternal"},
      {
          "org.telegram.messenger",
          "org.telegram.ui.LaunchActivity",
      },
      {"com.facebook.katana",
       "com.facebook.composer.shareintent."
       "ImplicitShareIntentHandlerDefaultAlias"},
      {
          "com.tencent.mm",
          "com.tencent.mm.ui.tools.ShareToTimeLineUI",
      },
      {
          "com.google.android.talk",
          "com.google.android.apps.hangouts.phone.ShareIntentActivity",
      },
      // TODO(crbug.com/40777590): Instagram Chat
      {
          "jp.naver.line.android",
          "com.linecorp.line.share.common.view.FullPickerLaunchActivity",
      },
      {
          "com.google.android.keep",
          "com.google.android.keep.activities.ShareReceiverActivity",
      },
      {
          "com.snapchat.android",
          "com.snap.mushroom.MainActivity",
      },
      {
          "com.twitter.android",
          "com.twitter.composer.ComposerActivity",
      },
      {"com.discord", "com.discord.app.AppActivity$AppAction"},
      // TODO(crbug.com/40777253): Whatsapp Business
      {
          "com.ideashower.readitlater.pro",
          "com.ideashower.readitlater.activity.AddActivity",
      },
      {
          "com.google.android.apps.docs.editors.docs",
          "com.google.android.apps.docs.common.shareitem.UploadMenuActivity",
      },
      {
          "com.pinterest",
          "com.pinterest.activity.create.PinItActivity",
      },
      {
          "com.linkedin.android",
          "com.linkedin.android.publishing.sharing.SharingDeepLinkActivity",
      },
      {
          "com.facebook.mlite",
          "com.facebook.mlite.share.view.ShareActivity",
      },
      {
          "org.thoughtcrime.securesms",
          "org.thoughtcrime.securesms.sharing.ShareActivity",
      },
      {
          "com.twitter.android",
          "com.twitter.app.dm.DMActivity",
      },
      {
          "com.facebook.lite",
          "com.facebook.lite.stories.activities.ShareToFbStoriesAlias",
      },
      {
          "com.microsoft.office.outlook",
          "com.microsoft.office.outlook.compose.ComposeLauncherActivity",
      },
      {
          "com.yahoo.mobile.client.android.mail",
          "com.yahoo.mail.flux.ui.MailComposeActivity",
      },
      {
          "com.viber.voip",
          "com.viber.voip.WelcomeShareActivity",
      },
      {
          "com.imo.android.imoim",
          "com.imo.android.imoim.globalshare.SharingActivity2",
      },
      {
          "com.reddit.frontpage",
          "com.reddit.sharing.ShareActivity",
      },
      // TODO(crbug.com/40777590): Samsung email
      {
          "com.tencent.mm",
          "com.tencent.mm.ui.tools.ShareImgUI",
      },
      {
          "com.verizon.messaging.vzmsgs",
          "com.verizon.mms.ui.LaunchConversationActivity",
      },
      {
          "com.google.android.apps.dynamite",
          "com.google.android.apps.dynamite.activity.main.MainActivity",
      },
  };
}

bool IsImageType(const std::string& type) {
  // TODO(ellyjones): The type string here comes from the Android side, which is
  // where the decision about image vs other is made - should we use a constant
  // for this, or an enum of ints instead?
  return type == "image";
}

bool IsEnUsLocale(const std::string& locale) {
  // TODO(ellyjones): This seems very unprincipled.
  return base::StartsWith(locale, "en_US");
}

std::vector<std::string> FlattenComponents(
    const std::vector<ComponentName>& cs) {
  std::vector<std::string> result;
  base::ranges::transform(cs, std::back_inserter(result),
                          &ComponentName::Flatten);
  return result;
}

}  // namespace

std::vector<std::string> DefaultRankingForLocaleAndType(
    const std::string& locale,
    const std::string& type) {
  if (IsEnUsLocale(locale)) {
    if (IsImageType(type))
      return FlattenComponents(DefaultEnUsImageRanking());
    return FlattenComponents(DefaultEnUsTextRanking());
  }
  if (IsImageType(type))
    return FlattenComponents(DefaultWorldImageRanking());
  return FlattenComponents(DefaultWorldTextRanking());
}

}  // namespace sharing

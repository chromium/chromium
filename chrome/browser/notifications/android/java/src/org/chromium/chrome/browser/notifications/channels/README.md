# Notification Channels

Notification channels define the togglable categories shown in our notification
settings within Android settings UI in Android O and above. Channels also
provide properties for our notifications, such as whether they vibrate or
make a sound, and expose these settings to the user.

Starting with Android O, all notifications must be assigned to a registered
notification channel. We enforce this in the codebase by requiring all
notifications to be constructed using
`NotificationWrapperBuilderFactory.createNotificationWrapperBuilder`, which requires a
valid `ChannelId`.

For an up-to-date enumeration of what channels exist, see the
map of `ChannelId`s to `Channel`s in `ChromeChannelDefinitions.PredefinedChannels`.

[TOC]

## When should a new channel be added?

New channels for new types of notifications should be added with caution -
whilst they do provide finer-grain control for users, this should be traded
off against the risk of settings creep. A multitude of settings can be
confusing, and users may have to toggle multiple settings to achieve their
desired state. Additionally, it’s hard to go back once channels have been
split out without disregarding user preferences set on those channels.

Therefore, any proposed new channels should go through the Chrome UI review
process.

If in doubt, we recommend posting the notification to the generic Browser
channel (assuming the Browser channel properties are appropriate). A new channel
can always be split out in future if deemed necessary.

> **Note**: Any time a new type of notification is added, a new
`SystemNotificationType` should be added to `enums.xml` and
`NotificationUmaTracker.onNotificationShown` must be called with this new
 type whenever any notifications are shown, to collect UMA on how often the
 notifications are blocked. *It is not necessary to add a new channel
 for every new SystemNotificationType.*

## How to add a new notification channel

Firstly, decide **when** the channel should be created - the first time it is used, or on first
launch of the app? (UI review can help with this).

In both cases, take the following steps:

1. Add a new id to the `@ChannelId` intdef in `ChromeChannelDefinitions.java`
2. Add a failing test in `ChannelsInitializerTest.java` for your new channel's properties (you'll
 need a new string for the channel name)
3. To make the test pass (yay TDD), add a corresponding entry to `PredefinedChannels.MAP` in
`ChromeChannelDefinitions.java` with the correct channel properties
4. Create notifications via
`NotificationWrapperBuilderFactory.createNotificationWrapperBuilder`, passing the new
channel id (the custom builder will set the channel on the notification for
you, and ensure the channel is initialized before building it)
5. After posting a notification, call `NotificationUmaTracker.onNotificationShown`, passing the new
 channel id (along with your new `SystemNotificationType`, see above)

For channels that should be created on first launch of the app, some extra steps are required:
- Add the new channel to `PredefinedChannels.STARTUP` in `ChromeChannelDefinitions.java`
- Increment `CHANNELS_VERSION` in `ChromeChannelDefinitions.java`
- Update startup channel tests in `ChannelsInitializerTest.java` and `ChannelsUpdaterTest.java`.

Note: An optional 'description' field exists for notification channels.
While we don't currently have any descriptions for the existing ones, it's encouraged to add them
for newly created channels, where appropriate. See [the setDescription documentation](https://developer.android.com/reference/android/app/NotificationChannel.html#setDescription(java.lang.String)) for details.

## Testing

> **Important**: As of October 2017, instrumented channel tests are not run on trybots because
 these tests are restricted to Android O+, and there are no such devices in the waterfall yet (
 [Issue 763951](https://crbug.com/763951)). So when making changes you *must* check all the channel tests
 pass on an Android O device locally.


    autoninja -C out/AndroidDebug chrome_public_test_apk

    out/AndroidDebug/bin/run_chrome_public_test_apk --test-filter "*Channel*"


## How to deprecate a channel

Note, renaming an existing channel is free, just update the string and bump the
`CHANNELS_VERSION` in `ChromeChannelDefinitions.java` so that updaters pick up the
change.

To stop an existing channel showing up any more, follow the following steps:

1. Ensure any notifications previously associated with this channel no longer
exist, or are now sent to alternative channels.
2. Remove the channel's entry from `PredefinedChannels.MAP` in `ChromeChannelDefinitions.java` and
`PredefinedChannels.STARTUP` in `ChromeChannelDefinitions.java`
3. Move the channel id from the `@ChannelId` intdef in `ChromeChannelDefinitions.java` to the
`LEGACY_CHANNEL_IDS` array in `ChromeChannelDefinitions.java`
4. Increment `CHANNELS_VERSION` in `ChromeChannelDefinitions.java`
5. Update tests in `ChannelsInitializerTest.java` that refer to the old channel

This should only happen infrequently. Note a 'deleted channels' count in
the browser's system notification settings will appear & increment every time a
channel is deleted.


## Appendix: History of channels in Clank

In M58 we started with only two channels - Sites and Browser. Web notifications
were posted to the Sites channel and all other notifications from the browser
went to the Browser channel.

In M59 we split various more specific channels out of the Browser channel,
including Media, Incognito and Downloads. The Browser channel still exists as
a general catch-all category for notifications sent from the browser.

From M62 the Sites channel is deprecated and sites with notification permission
each get a dedicated channel, within the 'Sites' channel group.

## Further reading

- [Android notification channels documentation](https://developer.android.com/preview/features/notification-channels.html)
- [Design doc for Clank notification channels](https://docs.google.com/document/d/1K9pjvlHF1oANNI8TqZgy151tap9zs1KUr2qfBXo1s_4/edit?usp=sharing)

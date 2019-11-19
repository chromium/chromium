# Page Freezing Opt-In and Opt-Out

*** note
Freezing is already enabled on Mobile and doesn't honor the Opt-Out/Opt-In
presented on this page. We plan to support the Opt-Out/Opt-In on Mobile before
enabling Freezing on Desktop.
***

## What is page freezing?

Page freezing consists of preventing tasks from running in all frames of a page.
Pages can be frozen when they are not visible so they consume less power, CPU
and memory.

This is different from the
[feature policies](https://wicg.github.io/page-lifecycle/spec.html#feature-policies)
that allow individual frames to be frozen.

## What is the impact of page freezing on my site?

Once a page is frozen, it cannot run any tasks. In particular, callbacks
associated with DOM timers, XHR requests or the push API will not run until the
page is resumed. To verify the behavior of your site when it is frozen, go to
`chrome://discards` and click [Freeze] next to a background tab which contains
your site. See [Will my site be frozen?](#Will-my-site-be-frozen_) for how
Chrome tries to avoid frezing sites when this is likely to break functionality.

A page can be notified when it is frozen or resumed via the
[Page Lifecycle API](https://developers.google.com/web/updates/2018/07/page-lifecycle-api).

The active->frozen and frozen->active transitions are described more formally in
the [spec](https://wicg.github.io/page-lifecycle/spec.html).

## Will my site be frozen?

*Freezing isn't enabled on Chrome for Desktop yet.*

On desktop, a page will not be frozen if:

* The browser was opted out of the intervention via enterprise policy.
* The origin opted itself out of the intervention via origin trial.
* The origin was locally observed to play audio, update its favicon, update its
  title or display a persistent notification while backgrounded.
* There is insufficient local background observation time for the origin.
* The page is currently capturing user media (webcam, microphone, etc).
* The page is currently being mirrored (casting, etc).
* The page is currently playing audio.
* The page is currently using WebUSB.
* The page is currently visible.
* The page is currently being inspected by DevTools.
* The page is currently capturing a window or screen.
* The page is currently holding a Web Lock or an IndexedDB transaction.
* The page is sharing its BrowsingInstance with another page.

Pages which meet one of the above criteria are the ones that are the most likely
to work incorrectly when frozen, which is why they are automatically opted-out.
It is possible that a page will work incorrectly when frozen even if it doesn't
meet these criteria. In that case, it should be explicitly opted-out from
freezing. See
[How to opt-out a site from freezing?](#How-to-opt-out-a-site-from-freezing_).

TODO(fdoray): Discuss how priority boosting will temporarily unfreeze a page
when another page tries to communicate with it.

## How to opt-out a site from freezing?

*It is not possible to register for the freezing opt-out yet.*

If page freezing breaks a functionality on your site, consider re-implementing
the functionality. For example, instead of polling a server from a DOM timer to
display notifications, use the
[push and notifications APIs from a service worker](https://developers.google.com/web/ilt/pwa/introduction-to-push-notifications).

There are some functionalities that currently cannot be implemented in a
freezing-friendly way. For example, it is not possible to update the title or
favicon of a page when it is frozen. Over time, we plan to introduce new
capabilities in service workers to eliminate all such cases.

If re-implementing a functionality of your site in a freezing-friendly way is
not possible, setup the Freezing Opt-Out trial on your site.

* [Register for the opt-out trial (Coming soon!)](https://developers.chrome.com/origintrials/#/trials/active)
* [What is an opt-out trial?](https://github.com/GoogleChrome/OriginTrials/blob/gh-pages/developer-guide.md#14-are-there-different-types-of-trials)

As new capabilities are added to service workers to allow implementation of more
functionalities in a freezing-friendly way, the current Freezing Opt-Out will be
discontinued and replaced with more restricted opt-outs.

## How to opt-in a site from freezing?

*It is not possible to register for the freezing opt-in yet.*

If you know that your site can safely be frozen, setup the Freezing Opt-In trial
on your site. This will allow Chrome to freeze your site without first observing
its behavior in the background. This will help reduce power, CPU and memory
usage.

* [Register for the opt-in trial (Coming soon!)](https://developers.chrome.com/origintrials/#/trials/active)
* [What is an opt-in trial?](https://github.com/GoogleChrome/OriginTrials/blob/gh-pages/developer-guide.md#14-are-there-different-types-of-trials)

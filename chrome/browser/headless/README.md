# Chromium’s new Headless mode

Headless Chromium allows running Chromium in a headless/server environment.
Expected use cases include loading web pages, extracting metadata (e.g., the
DOM) and generating bitmaps from page contents — using all the modern web
platform features provided by Chromium and Blink.

This directory hosts [the new Headless implementation](https://developer.chrome.com/articles/new-headless/),
sharing browser code in `//chrome`. The old Headless was implemented as a
separate application layer and can be found in `//headless`.

## Resources and documentation

Mailing list: [headless-dev@chromium.org](https://groups.google.com/a/chromium.org/g/headless-dev)

Bug tracker: [Internals>Headless](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=component%3AInternals%3EHeadless)

[File a new bug](https://bugs.chromium.org/p/chromium/issues/entry?components=Internals%3EHeadless)

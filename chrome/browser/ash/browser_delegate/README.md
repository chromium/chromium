This is an evolving abstraction of the `Browser` (chrome/browser/ui/browser.h)
and `BrowserList` (chrome/browser/ui/browser\_list.h) classes for use by
ChromeOS feature code.

The abstraction consists of two (C++) interfaces, `BrowserDelegate` &
`BrowserController`, and their implementation. Think of `BrowserDelegate` as an
abstract version of the existing `Browser` class and `BrowserController` as a
supervising entity that primarily creates or finds `BrowserDelegate` objects for
you.

Development is incremental and in the first phase we prioritize making these
classes the bottleneck for `Browser` access over them offering a clean API.

We aim to eventually move these interfaces out of chrome/browser/ash/ into
chromeos/.

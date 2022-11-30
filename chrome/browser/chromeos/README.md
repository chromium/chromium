chrome/browser/chromeos
=======================

This directory should contain Chrome OS specific code that has `//chrome`
dependencies.

This directory is for shared code between Ash and Lacros. Code that is only
used by Lacros should be in chrome/browser/lacros/ and code that is only used
by Ash should be in chrome/browser/ash/.

There are a few exceptions to the above rules while the code is being
migrated, e.g. c/b/c/exceptions/ and c/b/c/fileapi/ which are being actively
worked on to separate platform-specific code to the proper directories. See
the "Lacros: ChromeOS source code directory migration" design doc at
https://docs.google.com/document/d/1g-98HpzA8XcoGBWUv1gQNr4rbnD5yfvbtYZyPDDbkaE.

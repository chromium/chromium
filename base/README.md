# What is this
Contains a written down set of principles and other information on //base.
Please add to it!

## About //base:

Chromium is a very mature project. Most things that are generally useful are
already here and things not here aren't generally useful.

Base is pulled into many projects. For example, various ChromeOS daemons. So
the bar for adding stuff is that it must have demonstrated wide
applicability. Prefer to add things closer to where they're used (i.e. "not
base"), and pull into base only when needed.  In a project our size,
sometimes even duplication is OK and inevitable.

Adding a new logging macro `DPVELOG_NE` is not more clear than just
writing the stuff you want to log in a regular logging statement, even
if it makes your calling code longer. Just add it to your own code.

If the code in question does not need to be used inside base, but will have
multiple consumers across the codebase, consider placing it in a new directory
under components/ instead.

## Qualifications for being in //base OWNERS
  * interest and ability to learn low level/high detail/complex c++ stuff
  * inclination to always ask why and understand everything (including external
    interactions like win32) rather than just hoping the author did it right
  * mentorship/experience
  * demonstrated good judgement (esp with regards to public APIs) over a length
    of time

Owners are added when a contributor has shown the above qualifications and
when they express interest. There isn't an upper bound on the number of OWNERS.

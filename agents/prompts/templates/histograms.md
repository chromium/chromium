# Histograms

//tools/metrics/histograms/README.md describes policies about logging histograms.
Whenever you add code that logs a histogram, or you need to update metadata
about a histogram, follow the guidelines in that file along with the following
clarifications:

* If you add code that logs a histogram, also add metadata about that histogram
  and any associated enums.

* Metadata about histograms goes in a file called "histograms.xml". Metadata
  about enums goes in a file called "enums.xml" in the same directory.
* The XML files go in a subdirectory of //tools/metrics/histograms/metadata.
  The name of the subdirectory is based on the first part of the histogram
  name. If it isn't clear what subdirectory to use, ask the user.

* Histograms should have at least two owners. The first owner is usually the
  user who gave you instructions. If possible the other owner should be a team.
  If it isn't clear what to use for the second owner, ask the user.

* When you add metadata for a histogram, make sure the expiry date is in the
  future. Set it to 3 months in the future unless there's a clearly better date
  to use.

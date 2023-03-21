# Baseline Profiles

This directory contains Human Readable Format (HRF) baseline profiles. Googlers:
see go/baseline-profiles-in-chrome for more details about how this is going to
be used.

## What are baseline profiles?

These are basically the successor/sibling of android cloud profiles where a list
of classes and methods are marked as (startup, hot, etc.) hinting at the ART to
precompile certain parts of the dex rather than JIT for performance reasons. See
https://developer.android.com/topic/performance/baselineprofiles/overview for
more details.

## What are HRF profiles?

HRF or Human Readable Format profiles is a text file containing methods and
classes of your app alongside flags (startup, hot, etc). This is fairly stable
across small code changes and thus can be committed to the repo/cipd. Using the
macrobenchmark library you can get an HRF profile for your app. The ART can't
use this though and it must be converted to a binary profile which is then
shipped with your apk/bundle.

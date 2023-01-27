# cc/slim

This directory contains the "slim" compositor. The initial goal is a
re-implementation of cc with only the features and requirements needed by the
Android browser compositor, and transition the Android browser compositor from
cc to slim compositor.

During the transition, cc/slim will have a similar API surface as cc, and
cc/slim will conditionally wrap cc types so that slim compositor can be
controlled via an experiment.

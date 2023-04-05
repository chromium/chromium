# What is this?

//base/profiler implements a
[statistical profiler](https://en.wikipedia.org/wiki/Profiling_(computer_programming)#Statistical_profilers)
for Chrome execution. It supports periodic sampling of thread stacks for the
purpose of understanding how frequently different parts of the Chrome code are
being executed. The profiler is used to collect execution information by UMA,
for broad-scale profiling, and by Chrometto, for targeted profiling during
tracing.


## Technical Overview

The primary entry point to this code is
[StackSamplingProfiler](stack_sampling_profiler.h). This class regularly
records the list of currently executing functions on a target thread. See
the comments above that function for an overview of how to use the profiler.

The details are very platform-specific, but the major sub-components are

* A dedicated thread is created to periodically wake up and sample the target
  thread. At each wake up:
  * A [StackCopier](stack_copier.h) copies the target thread's stack
    memory into a [StackBuffer](stack_buffer.h).
  * One or more [Unwinders](unwinder.h) take the memory blob in the StackBuffer
    and turn it into a list of function [Frames](frame.h). Every platform has
    a native unwinder to deal with C++ frames; there are also unwinders for
    V8's special frame layout and for Java frames.
  * Frames have the function instruction address and some module information
    from [ModuleCache](module_cache.h). This should be enough for a program
    with access to the original debug information to reconstruct the names of
    the functions in the stack. The actual conversion back to human-readable
    names is not part of this directory's code.
  * A subclass of [ProfileBuilder](profile_builder.h) is called with a vector
    of Frames corresponding to one stack. The various users of this code are
    responsible for implementing this subclass and recording the stacks in the
    manner they see fit.

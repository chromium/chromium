# cc/animation

[TOC]

## Overview

cc/animation provides animation support - generating output values (usually
visual properties) based on a predefined function and changing input values.
Currently the main clients of cc/animation are Blink and ui/, targeting
composited layers, but the code is intended to be agnostic of the client it is
supporting. Aspirationally we could eventually merge cc/animation and Blink
animation and have only a single animation system for all of Chromium.

This document covers two main topics. The first section explains how
cc/animation actually works: how animations are ticked, what animation curves
are, what the ownership model is, etc. Later sections document how other parts
of Chromium interact with cc/animation, most prominently Blink and ui/.

## cc/animation terminology

[Animation](https://cs.chromium.org/chromium/src/cc/animation/animation.h)
A cc::Animation is responsible for managing animating properties for a set of
targets. Each target is represented by a [KeyframeEffect][] and can be animating
multiple properties on that target; see KeyframeEffect below.

A particular Animation may not own all the KeyframeEffects for a given
target. Animation is only a grouping mechanism for related effects, and the
grouping relationship is defined by the client. It is also the client's
responsibility to deal with any conflicts that arise from animating the same
property of the same target across multiple Animations.

Each Animation has a copy on the impl thread, and will take care of
synchronizing to/from the impl thread when requested.

[SingleKeyframeEffectAnimation](https://cs.chromium.org/chromium/src/cc/animation/single_keyframe_effect_animation.h)
It is a sub-class of Animation that serves as a bridge between the cc animation
clients and cc because currently only a single keyframe effect per animation is
supported.

There is a 1:1 relationship between SingleKeyframeEffectAnimation and
the KeyframeEffect.

> NOTE: SingleKeyframeEffectAnimation is being deprecated.

[Keyframe model](https://codesearch.chromium.org/chromium/src/cc/animation/keyframe_model.h)
KeyframeModels contain the state necessary to 'play' (interpolate values from) an

[animation curve](https://codesearch.chromium.org/chromium/src/cc/animation/animation_curve.h),
which is a function that returns a value given an input time. Aside from the
animation curve itself, a keyframe model's state includes the run state (playing,
paused, etc), the start time, the current direction (forwards, reverse), etc.
An animation does not know or care what property is being animated, and holds
only an opaque identifier for the property to allow clients to map output values
to the correct properties.

[Keyframe effect][]
A KeyframeEffect owns a group of KeyframeModels for a single target (identified
by [PropertyToElementIdMap][]). It is responsible for managing the KeyframeModels'
running states (starting, running, paused, etc), as well as ticking the
KeyframeModels when it is requested to produce new outputs for a given time.

Note that a single KeyframeEffect may not own all the KeyframeModels for a
given target. KeyframeEffect is only a grouping mechanism for related
KeyframeModels. The commonality between keyframe models on the same target is
found via ElementAnimations - there is only one ElementAnimations for a given
target.

Group:
KeyframeModels that must be run together are called 'grouped' and have the same
group id. Grouped KeyframeModels are guaranteed to start at the same time and no
other KeyframeModels may animate any of the group's target properties until all
KeyframeModels in the group have finished animating. It's also guaranteed that
no two keyframe models within a keyframe effect that have both the same group id
and property id.

In general, KeyframeModels are grouped together based on their
[animation target](https://codesearch.chromium.org/chromium/src/cc/animation/animation_target.h)
(the entity whose property is being animated) and each such group is owned by an
[animation](https://codesearch.chromium.org/chromium/src/cc/animation/animation.h).
Note that there may be multiple animations with the same target (each
with a set of KeyframeModels for that target); the
[ElementAnimations](https://codesearch.chromium.org/chromium/src/cc/animation/element_animations.h)
class wraps the multiple animations and has a 1:1 relationship with
target entities.

`TODO(smcgruer): Why are ElementAnimations and Animations separate?`

In order to play an animation, input time values must be provided to the
animation curve and output values fed back into the animating entity. This is
called 'ticking' an animation and is the responsibility of the
[animation host](https://codesearch.chromium.org/chromium/src/cc/animation/animation_host.h).
The animation host has a list of currently ticking animations (i.e. those that have
any non-deleted animations), which it iterates through whenever it receives a
tick call from the client (along with a corresponding input time).  The
animations then call into their non-deleted animations, retrieving the
value from the animation curve.  As they are computed, output values are sent to
the target which is responsible for passing them to the client entity that is
being animated.

### Types of Animation Curve

As noted above, an animation curve is simply a function which converts an input
time value into some output value. Animation curves are categorized based on
their output type, and each such category can have multiple implementations that
provide different conversion functions. There are many categories of animation
curve, but some common ones are `FloatAnimationCurve`, `ColorAnimationCurve`,
and `TransformAnimationCurve`.

The most common implementation of the various animation curve categories are the
[keyframed animation curves](https://codesearch.chromium.org/chromium/src/cc/animation/keyframed_animation_curve.h).
These curves each have a set of keyframes which map a specific time to a
specific output value. Producing an output value for a given input time is then
a matter of identifying the two keyframes the time lies between, and
interpolating between the keyframe output values. (Or simply using a keyframe
output value directly, if the input time happens to line up exactly.) Exact
details of how each animation curve category is interpolated can be found in the
implementations.

There is one category of animation curve that stands somewhat apart, the
[scroll offset animation curve](https://codesearch.chromium.org/chromium/src/cc/animation/scroll_offset_animation_curve.h).
This curve converts the input time into a scroll offset, interpolating between
an initial scroll offset and an updateable target scroll offset. It has logic to
handle different types of scrolling such as programmatic, keyboard, and mouse
wheel scrolls.

### Animation Timelines

cc/animation has a concept of an
[animation timeline](https://codesearch.chromium.org/chromium/src/cc/animation/animation_timeline.h).
This should not be confused with the identically named Blink concept. In
cc/animation, animation timelines are an implementation detail - they hold the
animations and are responsible for syncing them to the impl thread (see
below), but do not participate in the ticking process in any way.

### Main/Impl Threads

One part of cc/animation that is not client agnostic is its support for the
[Chromium compositor thread](https://codesearch.chromium.org/chromium/src/cc/README.md).
Most of the cc/animation classes have a `PushPropertiesTo` method, in which they
synchronize necessary state from the main thread to the impl thread. It is
feasible that such support could be abstracted if necessary, but so far it has
not been required.

## Current cc/animation Clients

As noted above, the main clients of cc/animation are currently Blink for
accelerated web animations, and ui/ for accelerated user interface animations.
Both of these clients utilize
[cc::Layer](https://codesearch.chromium.org/chromium/src/cc/layers/layer.h)
as their animation entity and interact with cc/animation via the
[MutatorHostClient](https://codesearch.chromium.org/chromium/src/cc/trees/mutator_host_client.h)
interface (which is implemented by cc::LayerTreeHost and cc::LayerTreeHostImpl).
Recently a third client, chrome/browser/vr/, has started using cc/animations as
well. The vr/ client does not use cc::Layer as its animation entity.

`TODO(smcgruer): Summarize how vr/ uses cc/animation.`

### Supported Animatable Properties

As cc::Layers are just textures which are reused for performance, clients that
use composited layers as their animation entities are limited to animating
properties that do not cause content to be redrawn. For example, a composited
layer's opacity can be animated as promoted layers are aware of the content
behind them.  On the other hand we cannot animate layer width as changing the
width could modify layout - which then requires redrawing.

### Interaction between cc/animation and Blink

Blink is able to move compatible animations off the main thread by promoting
the animating element into a layer. The Blink
[Lifetime of a compositor animation](../../third_party/blink/renderer/core/animation/README.md#lifetime-of-a-compositor-animation)
document describes how composited animations are created in blink. Once a
compositor animation is created it is pushed through the commit cycle.

![new animation]

The lifetime of a newly started cc::Animation is roughly the following:

1. An update to style or a new animation triggers a new [BeginMainFrame][] via
   [ScheduleVisualUpdate][].
1. [blink::DocumentAnimations::UpdateAnimations][] calls [blink::Animation::PreCommit][]
   on each pending blink::Animation constructing the corresponding
   cc::Animation via [blink::Animation::CreateCompositorAnimation][] (attaching
   the animation to the cc::AnimationTimeline resulting in it being later pushed).
   The KeyframeEffects are constructed via [blink::Animation::StartAnimationOnCompositor][].
1. [AnimationHost::RegisterKeyframeEffectForElement][] creates a
   cc::ElementAnimations for the target `element_id` if one does not already
   exist. This ElementAnimations instance is shared by all animations with
   the same target and tracks the existence of the target.
1. During the commit, [cc::ElementAnimations::ElementRegistered][] is called on the
   main thread's AnimationHost either:
   - Before BlinkGenPropertyTrees, when a layer with the target `element_id` is
     registered.
   - After BlinkGenPropertyTrees, after a property tree node with the target
     `element_id` is created on the main thread LayerTreeHost's `property_trees_`.
   This begins ticking the attached KeyframeEffects and tracks that the element
   exists in the active layer / property tree.
1. [cc::LayerTreeHost::FinishCommitOnImplThread][] calls
   [cc::AnimationHost::PushPropertiesTo][] which results in
   [cc::AnimationTimeline::PushAttachedAnimationsToImplThread][] creating a
   cc::Animation on the compositor thread's AnimationTimeline for each animation
   missing from the compositor thread.
1. [cc::Animation::PushPropertiesTo][] is called on every animation on the timeline.
   When the `element_id` is pushed by [cc::KeyframeEffect::PushPropertiesTo][]
   [cc::Animation::AttachElementForKeyframeEffect][] creates a compositor side
   cc::ElementAnimations instance to track the existence of the element on the
   compositor. Since animations are pushed after the layer and property trees,
   the element should already exist on the pending tree. This will result in the
   animation being added to the ticking animations list.
1. Now the animation is ticking, meaning that [cc::Animation::Tick][] will be called
   every frame and update the pending property tree nodes.
1. When the pending tree is activated,
   [cc::AnimationHost::ActivateAnimations][] updates the keyframe effects and
   [cc::ElementAnimations::ElementRegistered][]
   is called for the newly added element id on the active tree, setting
   `has_element_in_active_list_`.
1. Subsequent animation ticks will now update the property nodes on the active
   tree.

[new animation]: images/new-animation.png
[BeginMainFrame]: https://cs.chromium.org/chromium/src/cc/trees/proxy_main.cc?type=cs&q=file:proxy_main%5C.cc+RequestMainFrameUpdate
[ScheduleVisualUpdate]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/frame/local_frame.cc?type=cs&q=file:local_frame%5C.cc+ScheduleVisualUpdate
[blink::DocumentAnimations::UpdateAnimations]: https://cs.chromium.org/search?q=function:blink::DocumentAnimations::UpdateAnimations+GetPendingAnimations
[blink::Animation::PreCommit]: https://cs.chromium.org/search?q=function:blink::PendingAnimations::Update+%5C-%5C>PreCommit%5C(&g=0&l=57
[blink::Animation::CreateCompositorAnimation]: https://cs.chromium.org/search?q=function:blink::Animation::CreateCompositorAnimation+%5E%5B+%5D*AttachCompositorTimeline
[blink::Animation::StartAnimationOnCompositor]: https://cs.chromium.org/search?q=function:blink::Animation::StartAnimationOnCompositor+%5C-%5C>StartAnimationOnCompositor
[AnimationHost::RegisterKeyframeEffectForElement]: https://cs.chromium.org/search?q=function:cc::AnimationHost::RegisterKeyframeEffectForElement+ElementAnimations::Create
[cc::ElementAnimations::ElementRegistered]: https://cs.chromium.org/search?q=function:cc::ElementAnimations::ElementRegistered+%5C!has_element_in_any_list
[cc::LayerTreeHost::FinishCommitOnImplThread]: https://cs.chromium.org/search?q=cc::LayerTreeHost::FinishCommitOnImplThread+file:%5C.cc
[cc::AnimationHost::PushPropertiesTo]: https://cs.chromium.org/search/?q=function:cc::LayerTreeHost::FinishCommitOnImplThread+%5C-%5C>PushPropertiesTo
[cc::AnimationTimeline::PushAttachedAnimationsToImplThread]: https://cs.chromium.org/search?q=function:cc::AnimationTimeline::PushAttachedAnimationsToImplThread+animation%5C-%5C>CreateImplInstance
[cc::Animation::PushPropertiesTo]: https://cs.chromium.org/search?q=cc::Animation::PushPropertiesTo+file:%5C.cc
[cc::KeyframeEffect::PushPropertiesTo]: https://cs.chromium.org/search?q=cc::KeyframeEffect::PushPropertiesTo+file:%5C.cc
[cc::Animation::AttachElementForKeyframeEffect]: https://cs.chromium.org/search?q=cc::Animation::AttachElementForKeyframeEffect+file:%5C.cc
[cc::Animation::Tick]: https://cs.chromium.org/search?q=cc::Animation::Tick+file:%5C.cc
[cc::AnimationHost::ActivateAnimations]: https://cs.chromium.org/search?q=cc::AnimationHost::ActivateAnimations+ActivateKeyframeEffects
[cc::ElementAnimations::ElementRegistered]: https://cs.chromium.org/search?q=cc::ElementAnimations::ElementRegistered+file:%5C.cc
[KeyframeEffect]: https://cs.chromium.org/chromium/src/cc/animation/keyframe_effect.h
[PropertyToElementIdMap]: https://cs.chromium.org/chromium/src/cc/trees/target_property.h?type=cs&g=0&l=42

`TODO(flackr): Document finishing / cancel / abort.`

### Interaction between cc/animation and ui/

`TODO(smcgruer): Write this.`

## Additional References

The Compositor Property Trees talk [slides](https://goo.gl/U4wXpW)
includes discussion on compositor animations.

The Project Heaviside [design document](https://goo.gl/pWaWyv)
and [slides](https://goo.gl/iFpk4R) provide history on the Chromium
and Blink animation system. The slides in particular include helpful
software architecture diagrams.

Smooth scrolling is implemented via animations. See also references to
"scroll offset" animations in the cc code
base. [Smooth Scrolling in Chromium](https://goo.gl/XXwAwk) provides
an overview of smooth scrolling. There is further class header
documentation in
Blink's
[platform/scroll](https://codesearch.chromium.org/chromium/src/third_party/blink/renderer/platform/scroll/)
directory.

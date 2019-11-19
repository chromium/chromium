# cc/

This directory contains a compositor, used in both the renderer and the
browser.  In the renderer, Blink is the client.  In the browser, both
ui and Android browser compositor are the clients.

The public API of the compositor is LayerTreeHost and Layer and its
derived types.  Embedders create a LayerTreeHost (single, multithreaded,
or synchronous) and then attach a tree of Layers to it.

When Layers are updated they request a commit, which takes the structure
of the tree of Layers, the data on each Layer, and the data of its host and
atomically pushes it all to a tree of LayerImpls and a LayerTreeHostImpl
and LayerTreeImpl.  The main thread (which owns the tree of Layers
and the embedder) is blocked during this commit operation.

The commit is from the main thread Layer tree to the pending tree in
multithreaded mode.  The pending tree is a staging tree for
rasterization.  When enough rasterization has completed for
invalidations, the pending tree is ready to activate.  Activate is an
analogous operation to commit, and pushes data from the pending tree to
the active tree.  The pending tree exists so that all of the updates
from the main thread can be displayed to the user atomically while
the previous frame can be scrolled or animated.

The single threaded compositor commits directly to the active
tree and then stops drawing until the content is ready to be drawn.

The active tree is responsible for drawing.  The Scheduler and its
SchedulerStateMachine decide when to draw (along with when to commit,
etc etc).  "Drawing" in a compositor consists of LayerImpl::AppendQuads
which batches up a set of DrawQuads and RenderPasses into a
CompositorFrame which is sent via a CompositorFrameSink.

CompositorFrames from individual compositors are sent to the
SurfaceManager (currently in the browser process).  The
SurfaceAggregator combines all CompositorFrames together and asks
the Display to finally draw the frame via Renderer, which is either
a viz::GLRenderer or a SoftwareRenderer, which finally draws the entire
composited browser contents into a backbuffer or a bitmap, respectively.

Design documents for the graphics stack can be found at
[chromium-graphics](https://www.chromium.org/developers/design-documents/chromium-graphics).

## Other Docs

* [How cc Works](../docs/how_cc_works.md)

## Glossaries

### Active CompositorFrame

### Active Tree
The set of layers and property trees that was/will be used to submit a
CompositorFrame from the layer compositor. Composited effects such as scrolling,
pinch, and animations are done by modifying the active tree, which allows for
producing and submitting a new CompositorFrame.

### CompositorFrame
A set of RenderPasses (which are a list of DrawQuads) along with metadata.
Conceptually this is the instructions (transforms, texture ids, etc) for how to
draw an entire scene which will be presented in a surface.

### CopyOutputRequest (or Copy Request)
A request for a texture (or bitmap) copy of some part of the compositor's
output. Such requests force the compositor to use a separate RenderPass for the
content to be copied, which allows it to do the copy operation once the
RenderPass has been drawn to.

### ElementID
Chosen by cc's clients and can be used as a stable identifier across updates.
For example, blink uses ElementIDs as a stable id for the object (opaque to cc)
that is responsible for a composited animation. Some additional information in
[element_id.h](https://codesearch.chromium.org/chromium/src/cc/paint/element_id.h)

### DirectRenderer
An abstraction that provides an API for the Display to draw a fully-aggregated
CompositorFrame to a physical output. Subclasses of it provide implementations
for various backends, currently GL or Software.

### Layer
A conceptual piece of content that can appear on screen and has some known
position with respect to the viewport.  The Layer class only is used on the
main thread.  This, along with LayerTreeHost, is the main API for the
compositor.

### LayerImpl
The same as Layer, but on the compositor thread.

### LayerTree

### Occlusion Culling
Avoiding work by skipping over things which are not visible due to being
occluded (hidden from sight by other opaque things in front of them). Most
commonly refers to skipping drawing (ie culling) of DrawQuads when other
DrawQuads will be in front and occluding them.

### Property Trees

See also presentations on [Compositor Property Trees](https://docs.google.com/presentation/d/1V7gCqKR-edNdRDv0bDnJa_uEs6iARAU2h5WhgxHyejQ/preview)
and [Blink Property Trees](https://docs.google.com/presentation/u/1/d/1ak7YVrJITGXxqQ7tyRbwOuXB1dsLJlfpgC4wP7lykeo/preview).

### Display
A controller class that takes CompositorFrames for each surface and draws them
to a physical output.

### Draw
Filling pixels in a physical output (technically could be to an offscreen
texture), but this is the final output of the display compositor.

### DrawQuad
A unit of work for drawing. Each DrawQuad has its own texture id, transform,
offset, etc.

### Shared Quad State
A shared set of states used by multiple draw quads. DrawQuads that are linked to
the same shared quad state will all use the same properties from it, with the
addition of things found on their individual DrawQuad structures.

### Render Pass
A list of DrawQuads which will all be drawn together into the same render target
(either a texture or physical output). Most times all DrawQuads are part of a
single RenderPass. Additional RenderPasses are used for effects that require a
set of DrawQuads to be drawn together into a buffer first, with the effect
applied then to the buffer instead of each individual DrawQuad.

### Render Surface
Synonym for RenderPass now. Historically part of the Layer tree data structures,
with a 1:1 mapping to RenderPasses. RenderSurfaceImpl is a legacy piece that
remains.

### Surface

### Record

### Raster

### Paint

### Pending CompositorFrame

### Pending Tree
The set of layers and property trees that is generated from a main frame (or
BeginMainFrame, or commit). The pending tree exists to do raster work in the
layer compositor without clobbering the active tree until it is done. This
allows the active tree to be used in the meantime.

### Composite
To produce a single graphical output from multiple inputs. In practice, the
layer compositor does raster from recordings and manages memory, performs
composited effects such as scrolling, pinch, animations, producing a
CompositorFrame. The display compositor does an actual "composite" to draw the
final output into a single physical output.

### Invalidation
Invalidation is a unit of content update.  Any content updates from
Blink or ui must be accompanied by an invalidation to tell the compositor
that a piece of content must be rerasterized.  For example, if a 10x10
div with a background color has its width increased by 5 pixels, then
there will be a 5x10 invalidation (at least) for the new space covered
by the larger div.

Ideally, invalidations represent the minimum amount of content that must
be rerastered from the previous frame.  They are passed to the compositor
via Layer::SetNeedsDisplay(Rect).  Invalidation is tracked both to
minimize the amount of raster work needed, but also to allow for
partial raster of Tiles.  Invalidations also eventually become damage.

### Damage
Damage is the equivalent of invalidation, but for the final display.
As invalidation is the difference between two frames worth of content,
damage is the difference between two CompositorFrames.  Damage is
tracked via the DamageTracker.  This allows for partial swap, where
only the parts of the final CompositorFrame that touch the screen
are drawn, and only that drawn portion is swapped, which saves quite
a bit of power for small bits of damage.

Invalidation creates damage, in that if a piece of content updates, then
that content invalidation creates damage on screen.  Other things that
cause damage are analogous operations to invalidations, but on Layers.
For example, moving a Layer around, changing properties of Layers (e.g.
opacity), and adding/removing/reordering Layers will all create damage
(aka screen updates) but do not create invalidations (aka raster work).

### Tiles
An abstraction of a piece of content of a Layer.  A tile may be
rasterized or not.  It may be known to be a solid color or not.
A PictureLayerImpl indirectly owns a sparse set of Tiles to
represent its rasterizable content.  When tiles are invalidated,
they are replaced with new tiles.

### Prepare Tiles
Prioritize and schedule needed tiles for raster. This is the entry point to a
system that converts painting (raster sources / recording sources) into
rasterized resources that live on tiles. This also kicks off any dependent image
decodes for images that need to be decode for the raster to take place.

### Device Scale Factor
The scale at which we want to display content on the output device. For very
high resolution monitors, everything would become too small if just presented
1:1 with the pixels. So we use a larger number of physical pixels per logical
pixels. This ratio is the device scale factor. 1 or 2 is the most common on
ChromeOS. Values between 1 and 2 are common on Windows.

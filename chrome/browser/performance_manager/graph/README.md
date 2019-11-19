# Threading Model

The [graph](graph.h) can only be accessed from a single sequence.

Nodes can be created on any sequence, but as soon as they're added to a graph,
they can only be used on the graph's sequence, with the exception of the id
accessor.

# Node Lifetime

With the exception of the system node, which is a singleton, the GraphImpl does
not own nodes. The user of the graph is responsible for maintaining node
ownership. Any node that has been added to the graph, must be removed from the
graph with GraphImpl::RemoveNode before it's deleted. All nodes must be removed
from the graph before the graph is destroyed.
